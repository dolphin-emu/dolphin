// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace ExpressionParser
{

class ControlQualifier
{
public:
	bool has_device;
	Core::DeviceQualifier device_qualifier;
	std::string control_name;

	ControlQualifier() : has_device(false) {}

	operator std::string()
	{
		if (has_device)
			return device_qualifier.ToString() + ":" + control_name;
		else
			return control_name;
	}
};

class ControlFinder
{
public:
	ControlFinder(const Core::DeviceContainer &container_, const Core::DeviceQualifier &default_, const bool is_input_) : container(container_), default_device(default_), is_input(is_input_) {}
	Core::Device::Control *FindControl(ControlQualifier qualifier);

private:
	Core::Device *FindDevice(ControlQualifier qualifier);
	const Core::DeviceContainer &container;
	const Core::DeviceQualifier &default_device;
	bool is_input;
};

class ExpressionNode;
class Expression
{
public:
	Expression() : node(nullptr) {}
	Expression(ExpressionNode *node);
	~Expression();
	ControlState GetValue();
	void SetValue (ControlState state);
	int num_controls;
	ExpressionNode *node;
};

enum ExpressionParseStatus
{
	EXPRESSION_PARSE_SUCCESS = 0,
	EXPRESSION_PARSE_SYNTAX_ERROR,
	EXPRESSION_PARSE_NO_DEVICE,
};

ExpressionParseStatus ParseExpression(const std::string& expr, ControlFinder &finder, Expression **expr_out);

}
}
