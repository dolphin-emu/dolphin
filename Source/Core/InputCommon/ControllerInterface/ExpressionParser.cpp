// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/ExpressionParser.h"

using namespace ciface::Core;

namespace ciface
{
namespace ExpressionParser
{

enum TokenType
{
	TOK_DISCARD,
	TOK_INVALID,
	TOK_EOF,
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_AND,
	TOK_OR,
	TOK_NOT,
	TOK_ADD,
	TOK_CONTROL,
};

inline std::string OpName(TokenType op)
{
	switch (op)
	{
	case TOK_AND:
		return "And";
	case TOK_OR:
		return "Or";
	case TOK_NOT:
		return "Not";
	case TOK_ADD:
		return "Add";
	default:
		assert(false);
		return "";
	}
}

class Token
{
public:
	TokenType type;
	ControlQualifier qualifier;

	Token(TokenType type_) : type(type_) {}
	Token(TokenType type_, ControlQualifier qualifier_) : type(type_), qualifier(qualifier_) {}

	operator std::string()
	{
		switch (type)
		{
		case TOK_INVALID:
			return "Invalid";
		case TOK_DISCARD:
			return "Discard";
		case TOK_EOF:
			return "EOF";
		case TOK_LPAREN:
			return "(";
		case TOK_RPAREN:
			return ")";
		case TOK_AND:
			return "&";
		case TOK_OR:
			return "|";
		case TOK_NOT:
			return "!";
		case TOK_ADD:
			return "+";
		case TOK_CONTROL:
			return "Device(" + (std::string)qualifier + ")";
		}
	}
};

class Lexer {
public:
	std::string expr;
	std::string::iterator it;

	Lexer(std::string expr_) : expr(expr_)
	{
		it = expr.begin();
	}

	bool FetchBacktickString(std::string &value, char otherDelim = 0)
	{
		value = "";
		while (it != expr.end())
		{
			char c = *it;
			++it;
			if (c == '`')
				return false;
			if (c > 0 && c == otherDelim)
				return true;
			value += c;
		}
		return false;
	}

	Token GetFullyQualifiedControl()
	{
		ControlQualifier qualifier;
		std::string value;

		if (FetchBacktickString(value, ':'))
		{
			// Found colon, this is the device name
			qualifier.has_device = true;
			qualifier.device_qualifier.FromString(value);
			FetchBacktickString(value);
		}

		qualifier.control_name = value;

		return Token(TOK_CONTROL, qualifier);
	}

	Token GetBarewordsControl(char c)
	{
		std::string name;
		name += c;

		while (it != expr.end())
		{
			c = *it;
			if (!isalpha(c))
				break;
			name += c;
			++it;
		}

		ControlQualifier qualifier;
		qualifier.control_name = name;
		return Token(TOK_CONTROL, qualifier);
	}

	Token NextToken()
	{
		if (it == expr.end())
			return Token(TOK_EOF);

		char c = *it++;
		switch (c)
		{
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			return Token(TOK_DISCARD);
		case '(':
			return Token(TOK_LPAREN);
		case ')':
			return Token(TOK_RPAREN);
		case '&':
			return Token(TOK_AND);
		case '|':
			return Token(TOK_OR);
		case '!':
			return Token(TOK_NOT);
		case '+':
			return Token(TOK_ADD);
		case '`':
			return GetFullyQualifiedControl();
		default:
			if (isalpha(c))
				return GetBarewordsControl(c);
			else
				return Token(TOK_INVALID);
		}
	}

	ExpressionParseStatus Tokenize(std::vector<Token> &tokens)
	{
		while (true)
		{
			Token tok = NextToken();

			if (tok.type == TOK_DISCARD)
				continue;

			if (tok.type == TOK_INVALID)
			{
				tokens.clear();
				return EXPRESSION_PARSE_SYNTAX_ERROR;
			}

			tokens.push_back(tok);

			if (tok.type == TOK_EOF)
				break;
		}
		return EXPRESSION_PARSE_SUCCESS;
	}
};

class ExpressionNode
{
public:
	virtual ~ExpressionNode() {}
	virtual ControlState GetValue() { return 0; }
	virtual void SetValue(ControlState state) {}
	virtual int CountNumControls() { return 0; }
	virtual operator std::string() { return ""; }
};

class ControlExpression : public ExpressionNode
{
public:
	ControlQualifier qualifier;
	Device::Control *control;

	ControlExpression(ControlQualifier qualifier_, Device::Control *control_) : qualifier(qualifier_), control(control_) {}

	virtual ControlState GetValue() override
	{
		return control->ToInput()->GetGatedState();
	}

	virtual void SetValue(ControlState value) override
	{
		control->ToOutput()->SetGatedState(value);
	}

	virtual int CountNumControls() override
	{
		return 1;
	}

	virtual operator std::string() override
	{
		return "`" + (std::string)qualifier + "`";
	}
};

class BinaryExpression : public ExpressionNode
{
public:
	TokenType op;
	ExpressionNode *lhs;
	ExpressionNode *rhs;

	BinaryExpression(TokenType op_, ExpressionNode *lhs_, ExpressionNode *rhs_) : op(op_), lhs(lhs_), rhs(rhs_) {}
	virtual ~BinaryExpression()
	{
		delete lhs;
		delete rhs;
	}

	virtual ControlState GetValue() override
	{
		ControlState lhsValue = lhs->GetValue();
		ControlState rhsValue = rhs->GetValue();
		switch (op)
		{
		case TOK_AND:
			return std::min(lhsValue, rhsValue);
		case TOK_OR:
			return std::max(lhsValue, rhsValue);
		case TOK_ADD:
			return std::min(lhsValue + rhsValue, 1.0);
		default:
			assert(false);
			return 0;
		}
	}

	virtual void SetValue(ControlState value) override
	{
		// Don't do anything special with the op we have.
		// Treat "A & B" the same as "A | B".
		lhs->SetValue(value);
		rhs->SetValue(value);
	}

	virtual int CountNumControls() override
	{
		return lhs->CountNumControls() + rhs->CountNumControls();
	}

	virtual operator std::string() override
	{
		return OpName(op) + "(" + (std::string)(*lhs) + ", " + (std::string)(*rhs) + ")";
	}
};

class UnaryExpression : public ExpressionNode
{
public:
	TokenType op;
	ExpressionNode *inner;

	UnaryExpression(TokenType op_, ExpressionNode *inner_) : op(op_), inner(inner_) {}
	virtual ~UnaryExpression()
	{
		delete inner;
	}

	virtual ControlState GetValue() override
	{
		ControlState value = inner->GetValue();
		switch (op)
		{
		case TOK_NOT:
			return 1.0 - value;
		default:
			assert(false);
			return 0;
		}
	}

	virtual void SetValue(ControlState value) override
	{
		switch (op)
		{
		case TOK_NOT:
			inner->SetValue(1.0 - value);
		default:
			assert(false);
		}
	}

	virtual int CountNumControls() override
	{
		return inner->CountNumControls();
	}

	virtual operator std::string() override
	{
		return OpName(op) + "(" + (std::string)(*inner) + ")";
	}
};

Device *ControlFinder::FindDevice(ControlQualifier qualifier)
{
	if (qualifier.has_device)
		return container.FindDevice(qualifier.device_qualifier);
	else
		return container.FindDevice(default_device);
}

Device::Control *ControlFinder::FindControl(ControlQualifier qualifier)
{
	Device *device = FindDevice(qualifier);
	if (!device)
		return nullptr;

	if (is_input)
		return device->FindInput(qualifier.control_name);
	else
		return device->FindOutput(qualifier.control_name);
}

class Parser
{
public:

	Parser(std::vector<Token> tokens_, ControlFinder &finder_) : tokens(tokens_), finder(finder_)
	{
		m_it = tokens.begin();
	}

	ExpressionParseStatus Parse(Expression **expr_out)
	{
		ExpressionNode *node;
		ExpressionParseStatus status = Toplevel(&node);
		if (status != EXPRESSION_PARSE_SUCCESS)
			return status;

		*expr_out = new Expression(node);
		return EXPRESSION_PARSE_SUCCESS;
	}

private:
	std::vector<Token> tokens;
	std::vector<Token>::iterator m_it;
	ControlFinder &finder;

	Token Chew()
	{
		return *m_it++;
	}

	Token Peek()
	{
		return *m_it;
	}

	bool Expects(TokenType type)
	{
		Token tok = Chew();
		return tok.type == type;
	}

	ExpressionParseStatus Atom(ExpressionNode **expr_out)
	{
		Token tok = Chew();
		switch (tok.type)
		{
		case TOK_CONTROL:
			{
				Device::Control *control = finder.FindControl(tok.qualifier);
				if (control == nullptr)
					return EXPRESSION_PARSE_NO_DEVICE;

				*expr_out = new ControlExpression(tok.qualifier, control);
				return EXPRESSION_PARSE_SUCCESS;
			}
		case TOK_LPAREN:
			return Paren(expr_out);
		default:
			return EXPRESSION_PARSE_SYNTAX_ERROR;
		}
	}

	bool IsUnaryExpression(TokenType type)
	{
		switch (type)
		{
		case TOK_NOT:
			return true;
		default:
			return false;
		}
	}

	ExpressionParseStatus Unary(ExpressionNode **expr_out)
	{
		ExpressionParseStatus status;

		if (IsUnaryExpression(Peek().type))
		{
			Token tok = Chew();
			ExpressionNode *atom_expr;
			if ((status = Atom(&atom_expr)) != EXPRESSION_PARSE_SUCCESS)
				return status;
			*expr_out = new UnaryExpression(tok.type, atom_expr);
			return EXPRESSION_PARSE_SUCCESS;
		}

		return Atom(expr_out);
	}

	bool IsBinaryToken(TokenType type)
	{
		switch (type)
		{
		case TOK_AND:
		case TOK_OR:
		case TOK_ADD:
			return true;
		default:
			return false;
		}
	}

	ExpressionParseStatus Binary(ExpressionNode **expr_out)
	{
		ExpressionParseStatus status;

		if ((status = Unary(expr_out)) != EXPRESSION_PARSE_SUCCESS)
			return status;

		while (IsBinaryToken(Peek().type))
		{
			Token tok = Chew();
			ExpressionNode *unary_expr;
			if ((status = Unary(&unary_expr)) != EXPRESSION_PARSE_SUCCESS)
			{
				delete *expr_out;
				return status;
			}

			*expr_out = new BinaryExpression(tok.type, *expr_out, unary_expr);
		}

		return EXPRESSION_PARSE_SUCCESS;
	}

	ExpressionParseStatus Paren(ExpressionNode **expr_out)
	{
		ExpressionParseStatus status;

		// lparen already chewed
		if ((status = Toplevel(expr_out)) != EXPRESSION_PARSE_SUCCESS)
			return status;

		if (!Expects(TOK_RPAREN))
		{
			delete *expr_out;
			return EXPRESSION_PARSE_SYNTAX_ERROR;
		}

		return EXPRESSION_PARSE_SUCCESS;
	}

	ExpressionParseStatus Toplevel(ExpressionNode **expr_out)
	{
		return Binary(expr_out);
	}
};

ControlState Expression::GetValue()
{
	return node->GetValue();
}

void Expression::SetValue(ControlState value)
{
	node->SetValue(value);
}

Expression::Expression(ExpressionNode *node_)
{
	node = node_;
	num_controls = node->CountNumControls();
}

Expression::~Expression()
{
	delete node;
}

static ExpressionParseStatus ParseExpressionInner(std::string str, ControlFinder &finder, Expression **expr_out)
{
	ExpressionParseStatus status;
	Expression *expr;
	*expr_out = nullptr;

	if (str == "")
		return EXPRESSION_PARSE_SUCCESS;

	Lexer l(str);
	std::vector<Token> tokens;
	status = l.Tokenize(tokens);
	if (status != EXPRESSION_PARSE_SUCCESS)
		return status;

	Parser p(tokens, finder);
	status = p.Parse(&expr);
	if (status != EXPRESSION_PARSE_SUCCESS)
		return status;

	*expr_out = expr;
	return EXPRESSION_PARSE_SUCCESS;
}

ExpressionParseStatus ParseExpression(std::string str, ControlFinder &finder, Expression **expr_out)
{
	// Add compatibility with old simple expressions, which are simple
	// barewords control names.

	ControlQualifier qualifier;
	qualifier.control_name = str;
	qualifier.has_device = false;

	Device::Control *control = finder.FindControl(qualifier);
	if (control)
	{
		*expr_out = new Expression(new ControlExpression(qualifier, control));
		return EXPRESSION_PARSE_SUCCESS;
	}

	return ParseExpressionInner(str, finder, expr_out);
}

}
}
