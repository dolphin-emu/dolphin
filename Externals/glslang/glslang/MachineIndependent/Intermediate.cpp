//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2012-2015 LunarG, Inc.
//Copyright (C) 2015-2016 Google, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

//
// Build the intermediate representation.
//

#include "localintermediate.h"
#include "RemoveTree.h"
#include "SymbolTable.h"
#include "propagateNoContraction.h"

#include <float.h>

namespace glslang {

////////////////////////////////////////////////////////////////////////////
//
// First set of functions are to help build the intermediate representation.
// These functions are not member functions of the nodes.
// They are called from parser productions.
//
/////////////////////////////////////////////////////////////////////////////

//
// Add a terminal node for an identifier in an expression.
//
// Returns the added node.
//

TIntermSymbol* TIntermediate::addSymbol(int id, const TString& name, const TType& type, const TConstUnionArray& constArray,
                                        TIntermTyped* constSubtree, const TSourceLoc& loc)
{
    TIntermSymbol* node = new TIntermSymbol(id, name, type);
    node->setLoc(loc);
    node->setConstArray(constArray);
    node->setConstSubtree(constSubtree);

    return node;
}

TIntermSymbol* TIntermediate::addSymbol(const TVariable& variable)
{
    glslang::TSourceLoc loc; // just a null location
    loc.init();

    return addSymbol(variable, loc);
}

TIntermSymbol* TIntermediate::addSymbol(const TVariable& variable, const TSourceLoc& loc)
{
    return addSymbol(variable.getUniqueId(), variable.getName(), variable.getType(), variable.getConstArray(), variable.getConstSubtree(), loc);
}

TIntermSymbol* TIntermediate::addSymbol(const TType& type, const TSourceLoc& loc)
{
    TConstUnionArray unionArray;  // just a null constant

    return addSymbol(0, "", type, unionArray, nullptr, loc);
}

//
// Connect two nodes with a new parent that does a binary operation on the nodes.
//
// Returns the added node.
//
TIntermTyped* TIntermediate::addBinaryMath(TOperator op, TIntermTyped* left, TIntermTyped* right, TSourceLoc loc)
{
    // No operations work on blocks
    if (left->getType().getBasicType() == EbtBlock || right->getType().getBasicType() == EbtBlock)
        return 0;

    // Try converting the children's base types to compatible types.
    TIntermTyped* child = addConversion(op, left->getType(), right);
    if (child)
        right = child;
    else {
        child = addConversion(op, right->getType(), left);
        if (child)
            left = child;
        else
            return 0;
    }

    //
    // Need a new node holding things together.  Make
    // one and promote it to the right type.
    //
    TIntermBinary* node = new TIntermBinary(op);
    if (loc.line == 0)
        loc = right->getLoc();
    node->setLoc(loc);

    node->setLeft(left);
    node->setRight(right);
    if (! node->promote())
        return 0;

    node->updatePrecision();

    //
    // If they are both (non-specialization) constants, they must be folded.
    // (Unless it's the sequence (comma) operator, but that's handled in addComma().)
    //
    TIntermConstantUnion *leftTempConstant = left->getAsConstantUnion();
    TIntermConstantUnion *rightTempConstant = right->getAsConstantUnion();
    if (leftTempConstant && rightTempConstant) {
        TIntermTyped* folded = leftTempConstant->fold(node->getOp(), rightTempConstant);
        if (folded)
            return folded;
    }

    // If either is a specialization constant, while the other is 
    // a constant (or specialization constant), the result is still
    // a specialization constant, if the operation is an allowed
    // specialization-constant operation.
    if (( left->getType().getQualifier().isSpecConstant() && right->getType().getQualifier().isConstant()) ||
        (right->getType().getQualifier().isSpecConstant() &&  left->getType().getQualifier().isConstant()))
        if (isSpecializationOperation(*node))
            node->getWritableType().getQualifier().makeSpecConstant();

    return node;
}

//
// Connect two nodes through an assignment.
//
// Returns the added node.
//
TIntermTyped* TIntermediate::addAssign(TOperator op, TIntermTyped* left, TIntermTyped* right, TSourceLoc loc)
{
    // No block assignment
    if (left->getType().getBasicType() == EbtBlock || right->getType().getBasicType() == EbtBlock)
        return 0;

    //
    // Like adding binary math, except the conversion can only go
    // from right to left.
    //
    TIntermBinary* node = new TIntermBinary(op);
    if (loc.line == 0)
        loc = left->getLoc();
    node->setLoc(loc);

    TIntermTyped* child = addConversion(op, left->getType(), right);
    if (child == 0)
        return 0;

    node->setLeft(left);
    node->setRight(child);
    if (! node->promote())
        return 0;

    node->updatePrecision();

    return node;
}

//
// Connect two nodes through an index operator, where the left node is the base
// of an array or struct, and the right node is a direct or indirect offset.
//
// Returns the added node.
// The caller should set the type of the returned node.
//
TIntermTyped* TIntermediate::addIndex(TOperator op, TIntermTyped* base, TIntermTyped* index, TSourceLoc loc)
{
    TIntermBinary* node = new TIntermBinary(op);
    if (loc.line == 0)
        loc = index->getLoc();
    node->setLoc(loc);
    node->setLeft(base);
    node->setRight(index);

    // caller should set the type

    return node;
}

//
// Add one node as the parent of another that it operates on.
//
// Returns the added node.
//
TIntermTyped* TIntermediate::addUnaryMath(TOperator op, TIntermTyped* child, TSourceLoc loc)
{
    if (child == 0)
        return 0;

    if (child->getType().getBasicType() == EbtBlock)
        return 0;

    switch (op) {
    case EOpLogicalNot:
        if (child->getType().getBasicType() != EbtBool || child->getType().isMatrix() || child->getType().isArray() || child->getType().isVector()) {
            return 0;
        }
        break;

    case EOpPostIncrement:
    case EOpPreIncrement:
    case EOpPostDecrement:
    case EOpPreDecrement:
    case EOpNegative:
        if (child->getType().getBasicType() == EbtStruct || child->getType().isArray())
            return 0;
    default: break; // some compilers want this
    }

    //
    // Do we need to promote the operand?
    //
    TBasicType newType = EbtVoid;
    switch (op) {
    case EOpConstructInt:    newType = EbtInt;    break;
    case EOpConstructUint:   newType = EbtUint;   break;
    case EOpConstructInt64:  newType = EbtInt64;  break;
    case EOpConstructUint64: newType = EbtUint64; break;
    case EOpConstructBool:   newType = EbtBool;   break;
    case EOpConstructFloat:  newType = EbtFloat;  break;
    case EOpConstructDouble: newType = EbtDouble; break;
    default: break; // some compilers want this
    }

    if (newType != EbtVoid) {
        child = addConversion(op, TType(newType, EvqTemporary, child->getVectorSize(),
                                                               child->getMatrixCols(),
                                                               child->getMatrixRows()),
                              child);
        if (child == 0)
            return 0;
    }

    //
    // For constructors, we are now done, it was all in the conversion.
    // TODO: but, did this bypass constant folding?
    //
    switch (op) {
    case EOpConstructInt:
    case EOpConstructUint:
    case EOpConstructInt64:
    case EOpConstructUint64:
    case EOpConstructBool:
    case EOpConstructFloat:
    case EOpConstructDouble:
        return child;
    default: break; // some compilers want this
    }

    //
    // Make a new node for the operator.
    //
    TIntermUnary* node = new TIntermUnary(op);
    if (loc.line == 0)
        loc = child->getLoc();
    node->setLoc(loc);
    node->setOperand(child);

    if (! node->promote())
        return 0;

    node->updatePrecision();

    // If it's a (non-specialization) constant, it must be folded.
    if (child->getAsConstantUnion())
        return child->getAsConstantUnion()->fold(op, node->getType());

    // If it's a specialization constant, the result is too,
    // if the operation is allowed for specialization constants.
    if (child->getType().getQualifier().isSpecConstant() && isSpecializationOperation(*node))
        node->getWritableType().getQualifier().makeSpecConstant();

    return node;
}

TIntermTyped* TIntermediate::addBuiltInFunctionCall(const TSourceLoc& loc, TOperator op, bool unary, TIntermNode* childNode, const TType& returnType)
{
    if (unary) {
        //
        // Treat it like a unary operator.
        // addUnaryMath() should get the type correct on its own;
        // including constness (which would differ from the prototype).
        //
        TIntermTyped* child = childNode->getAsTyped();
        if (child == 0)
            return 0;

        if (child->getAsConstantUnion()) {
            TIntermTyped* folded = child->getAsConstantUnion()->fold(op, returnType);
            if (folded)
                return folded;
        }

        TIntermUnary* node = new TIntermUnary(op);
        node->setLoc(child->getLoc());
        node->setOperand(child);
        node->setType(returnType);

        // propagate precision up from child
        if (profile == EEsProfile && returnType.getQualifier().precision == EpqNone && returnType.getBasicType() != EbtBool)
            node->getQualifier().precision = child->getQualifier().precision;

        // propagate precision down to child
        if (node->getQualifier().precision != EpqNone)
            child->propagatePrecision(node->getQualifier().precision);

        return node;
    } else {
        // setAggregateOperater() calls fold() for constant folding
        TIntermTyped* node = setAggregateOperator(childNode, op, returnType, loc);

        // if not folded, we'll still have an aggregate node to propagate precision with
        if (node->getAsAggregate()) {
            TPrecisionQualifier correctPrecision = returnType.getQualifier().precision;
            if (correctPrecision == EpqNone && profile == EEsProfile) {
                // find the maximum precision from the arguments, for the built-in's return precision
                TIntermSequence& sequence = node->getAsAggregate()->getSequence();
                for (unsigned int arg = 0; arg < sequence.size(); ++arg)
                    correctPrecision = std::max(correctPrecision, sequence[arg]->getAsTyped()->getQualifier().precision);
            }

            // Propagate precision through this node and its children. That algorithm stops
            // when a precision is found, so start by clearing this subroot precision
            node->getQualifier().precision = EpqNone;
            node->propagatePrecision(correctPrecision);
        }

        return node;
    }
}

//
// This is the safe way to change the operator on an aggregate, as it
// does lots of error checking and fixing.  Especially for establishing
// a function call's operation on it's set of parameters.  Sequences
// of instructions are also aggregates, but they just directly set
// their operator to EOpSequence.
//
// Returns an aggregate node, which could be the one passed in if
// it was already an aggregate.
//
TIntermTyped* TIntermediate::setAggregateOperator(TIntermNode* node, TOperator op, const TType& type, TSourceLoc loc)
{
    TIntermAggregate* aggNode;

    //
    // Make sure we have an aggregate.  If not turn it into one.
    //
    if (node) {
        aggNode = node->getAsAggregate();
        if (aggNode == 0 || aggNode->getOp() != EOpNull) {
            //
            // Make an aggregate containing this node.
            //
            aggNode = new TIntermAggregate();
            aggNode->getSequence().push_back(node);
            if (loc.line == 0)
                loc = node->getLoc();
        }
    } else
        aggNode = new TIntermAggregate();

    //
    // Set the operator.
    //
    aggNode->setOperator(op);
    if (loc.line != 0)
        aggNode->setLoc(loc);

    aggNode->setType(type);

    return fold(aggNode);
}

//
// Convert the node's type to the given type, as allowed by the operation involved: 'op'.
// For implicit conversions, 'op' is not the requested conversion, it is the explicit
// operation requiring the implicit conversion.
//
// Returns a node representing the conversion, which could be the same
// node passed in if no conversion was needed.
//
// Return 0 if a conversion can't be done.
//
TIntermTyped* TIntermediate::addConversion(TOperator op, const TType& type, TIntermTyped* node) const
{
    //
    // Does the base type even allow the operation?
    //
    switch (node->getBasicType()) {
    case EbtVoid:
        return 0;
    case EbtAtomicUint:
    case EbtSampler:
        // opaque types can be passed to functions
        if (op == EOpFunction)
            break;
        // samplers can get assigned via a sampler constructor
        // (well, not yet, but code in the rest of this function is ready for it)
        if (node->getBasicType() == EbtSampler && op == EOpAssign && 
            node->getAsOperator() != nullptr && node->getAsOperator()->getOp() == EOpConstructTextureSampler)
            break;

        // otherwise, opaque types can't even be operated on, let alone converted
        return 0;
    default:
        break;
    }

    // Otherwise, if types are identical, no problem
    if (type == node->getType())
        return node;

    // If one's a structure, then no conversions.
    if (type.isStruct() || node->isStruct())
        return 0;

    // If one's an array, then no conversions.
    if (type.isArray() || node->getType().isArray())
        return 0;

    // Note: callers are responsible for other aspects of shape,
    // like vector and matrix sizes.

    TBasicType promoteTo;

    switch (op) {
    //
    // Explicit conversions (unary operations)
    //
    case EOpConstructBool:
        promoteTo = EbtBool;
        break;
    case EOpConstructFloat:
        promoteTo = EbtFloat;
        break;
    case EOpConstructDouble:
        promoteTo = EbtDouble;
        break;
    case EOpConstructInt:
        promoteTo = EbtInt;
        break;
    case EOpConstructUint:
        promoteTo = EbtUint;
        break;
    case EOpConstructInt64:
        promoteTo = EbtInt64;
        break;
    case EOpConstructUint64:
        promoteTo = EbtUint64;
        break;

    //
    // List all the binary ops that can implicitly convert one operand to the other's type;
    // This implements the 'policy' for implicit type conversion.
    //
    case EOpLessThan:
    case EOpGreaterThan:
    case EOpLessThanEqual:
    case EOpGreaterThanEqual:
    case EOpEqual:
    case EOpNotEqual:

    case EOpAdd:
    case EOpSub:
    case EOpMul:
    case EOpDiv:
    case EOpMod:

    case EOpVectorTimesScalar:
    case EOpVectorTimesMatrix:
    case EOpMatrixTimesVector:
    case EOpMatrixTimesScalar:

    case EOpAnd:
    case EOpInclusiveOr:
    case EOpExclusiveOr:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:

    case EOpFunctionCall:
    case EOpReturn:
    case EOpAssign:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpMulAssign:
    case EOpVectorTimesScalarAssign:
    case EOpMatrixTimesScalarAssign:
    case EOpDivAssign:
    case EOpModAssign:

    case EOpSequence:
    case EOpConstructStruct:

        if (type.getBasicType() == node->getType().getBasicType())
            return node;

        if (canImplicitlyPromote(node->getType().getBasicType(), type.getBasicType()))
            promoteTo = type.getBasicType();
        else
            return 0;

        break;

    // Shifts can have mixed types as long as they are integer, without converting.
    // It's the left operand's type that determines the resulting type, so no issue
    // with assign shift ops either.
    case EOpLeftShift:
    case EOpRightShift:
    case EOpLeftShiftAssign:
    case EOpRightShiftAssign:
        if ((type.getBasicType() == EbtInt ||
             type.getBasicType() == EbtUint ||
             type.getBasicType() == EbtInt64 ||
             type.getBasicType() == EbtUint64) &&
            (node->getType().getBasicType() == EbtInt ||
             node->getType().getBasicType() == EbtUint ||
             node->getType().getBasicType() == EbtInt64 ||
             node->getType().getBasicType() == EbtUint64))

            return node;
        else
            return 0;

    default:
        // default is to require a match; all exceptions should have case statements above

        if (type.getBasicType() == node->getType().getBasicType())
            return node;
        else
            return 0;
    }

    if (node->getAsConstantUnion())
        return promoteConstantUnion(promoteTo, node->getAsConstantUnion());

    //
    // Add a new newNode for the conversion.
    //
    TIntermUnary* newNode = 0;

    TOperator newOp = EOpNull;

    // This is 'mechanism' here, it does any conversion told.  The policy comes
    // from the shader or the above code.
    switch (promoteTo) {
    case EbtDouble:
        switch (node->getBasicType()) {
        case EbtInt:   newOp = EOpConvIntToDouble;   break;
        case EbtUint:  newOp = EOpConvUintToDouble;  break;
        case EbtBool:  newOp = EOpConvBoolToDouble;  break;
        case EbtFloat: newOp = EOpConvFloatToDouble; break;
        case EbtInt64: newOp = EOpConvInt64ToDouble; break;
        case EbtUint64: newOp = EOpConvUint64ToDouble; break;
        default:
            return 0;
        }
        break;
    case EbtFloat:
        switch (node->getBasicType()) {
        case EbtInt:    newOp = EOpConvIntToFloat;    break;
        case EbtUint:   newOp = EOpConvUintToFloat;   break;
        case EbtBool:   newOp = EOpConvBoolToFloat;   break;
        case EbtDouble: newOp = EOpConvDoubleToFloat; break;
        case EbtInt64:  newOp = EOpConvInt64ToFloat;  break;
        case EbtUint64: newOp = EOpConvUint64ToFloat; break;
        default:
            return 0;
        }
        break;
    case EbtBool:
        switch (node->getBasicType()) {
        case EbtInt:    newOp = EOpConvIntToBool;    break;
        case EbtUint:   newOp = EOpConvUintToBool;   break;
        case EbtFloat:  newOp = EOpConvFloatToBool;  break;
        case EbtDouble: newOp = EOpConvDoubleToBool; break;
        case EbtInt64:  newOp = EOpConvInt64ToBool;  break;
        case EbtUint64: newOp = EOpConvUint64ToBool; break;
        default:
            return 0;
        }
        break;
    case EbtInt:
        switch (node->getBasicType()) {
        case EbtUint:   newOp = EOpConvUintToInt;   break;
        case EbtBool:   newOp = EOpConvBoolToInt;   break;
        case EbtFloat:  newOp = EOpConvFloatToInt;  break;
        case EbtDouble: newOp = EOpConvDoubleToInt; break;
        case EbtInt64:  newOp = EOpConvInt64ToInt;  break;
        case EbtUint64: newOp = EOpConvUint64ToInt; break;
        default:
            return 0;
        }
        break;
    case EbtUint:
        switch (node->getBasicType()) {
        case EbtInt:    newOp = EOpConvIntToUint;    break;
        case EbtBool:   newOp = EOpConvBoolToUint;   break;
        case EbtFloat:  newOp = EOpConvFloatToUint;  break;
        case EbtDouble: newOp = EOpConvDoubleToUint; break;
        case EbtInt64:  newOp = EOpConvInt64ToUint;  break;
        case EbtUint64: newOp = EOpConvUint64ToUint; break;
        default:
            return 0;
        }
        break;
    case EbtInt64:
        switch (node->getBasicType()) {
        case EbtInt:    newOp = EOpConvIntToInt64;    break;
        case EbtUint:   newOp = EOpConvUintToInt64;   break;
        case EbtBool:   newOp = EOpConvBoolToInt64;   break;
        case EbtFloat:  newOp = EOpConvFloatToInt64;  break;
        case EbtDouble: newOp = EOpConvDoubleToInt64; break;
        case EbtUint64: newOp = EOpConvUint64ToInt64; break;
        default:
            return 0;
        }
        break;
    case EbtUint64:
        switch (node->getBasicType()) {
        case EbtInt:    newOp = EOpConvIntToUint64;    break;
        case EbtUint:   newOp = EOpConvUintToUint64;   break;
        case EbtBool:   newOp = EOpConvBoolToUint64;   break;
        case EbtFloat:  newOp = EOpConvFloatToUint64;  break;
        case EbtDouble: newOp = EOpConvDoubleToUint64; break;
        case EbtInt64:  newOp = EOpConvInt64ToUint64;  break;
        default:
            return 0;
        }
        break;
    default:
        return 0;
    }

    TType newType(promoteTo, EvqTemporary, node->getVectorSize(), node->getMatrixCols(), node->getMatrixRows());
    newNode = new TIntermUnary(newOp, newType);
    newNode->setLoc(node->getLoc());
    newNode->setOperand(node);

    // TODO: it seems that some unary folding operations should occur here, but are not

    // Propagate specialization-constant-ness, if allowed
    if (node->getType().getQualifier().isSpecConstant() && isSpecializationOperation(*newNode))
        newNode->getWritableType().getQualifier().makeSpecConstant();

    return newNode;
}

//
// See if the 'from' type is allowed to be implicitly converted to the
// 'to' type.  This is not about vector/array/struct, only about basic type.
//
bool TIntermediate::canImplicitlyPromote(TBasicType from, TBasicType to) const
{
    if (profile == EEsProfile || version == 110)
        return false;

    switch (to) {
    case EbtDouble:
        switch (from) {
        case EbtInt:
        case EbtUint:
        case EbtInt64:
        case EbtUint64:
        case EbtFloat:
        case EbtDouble:
            return true;
        default:
            return false;
        }
    case EbtFloat:
        switch (from) {
        case EbtInt:
        case EbtUint:
        case EbtFloat:
            return true;
        default:
            return false;
        }
    case EbtUint:
        switch (from) {
        case EbtInt:
            return version >= 400;
        case EbtUint:
            return true;
        default:
            return false;
        }
    case EbtInt:
        switch (from) {
        case EbtInt:
            return true;
        default:
            return false;
        }
    case EbtUint64:
        switch (from) {
        case EbtInt:
        case EbtUint:
        case EbtInt64:
        case EbtUint64:
            return true;
        default:
            return false;
        }
    case EbtInt64:
        switch (from) {
        case EbtInt:
        case EbtInt64:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

//
// Safe way to combine two nodes into an aggregate.  Works with null pointers,
// a node that's not a aggregate yet, etc.
//
// Returns the resulting aggregate, unless 0 was passed in for
// both existing nodes.
//
TIntermAggregate* TIntermediate::growAggregate(TIntermNode* left, TIntermNode* right)
{
    if (left == 0 && right == 0)
        return 0;

    TIntermAggregate* aggNode = 0;
    if (left)
        aggNode = left->getAsAggregate();
    if (! aggNode || aggNode->getOp() != EOpNull) {
        aggNode = new TIntermAggregate;
        if (left)
            aggNode->getSequence().push_back(left);
    }

    if (right)
        aggNode->getSequence().push_back(right);

    return aggNode;
}

TIntermAggregate* TIntermediate::growAggregate(TIntermNode* left, TIntermNode* right, const TSourceLoc& loc)
{
    TIntermAggregate* aggNode = growAggregate(left, right);
    if (aggNode)
        aggNode->setLoc(loc);

    return aggNode;
}

//
// Turn an existing node into an aggregate.
//
// Returns an aggregate, unless 0 was passed in for the existing node.
//
TIntermAggregate* TIntermediate::makeAggregate(TIntermNode* node)
{
    if (node == 0)
        return 0;

    TIntermAggregate* aggNode = new TIntermAggregate;
    aggNode->getSequence().push_back(node);
    aggNode->setLoc(node->getLoc());

    return aggNode;
}

TIntermAggregate* TIntermediate::makeAggregate(TIntermNode* node, const TSourceLoc& loc)
{
    if (node == 0)
        return 0;

    TIntermAggregate* aggNode = new TIntermAggregate;
    aggNode->getSequence().push_back(node);
    aggNode->setLoc(loc);

    return aggNode;
}

//
// For "if" test nodes.  There are three children; a condition,
// a true path, and a false path.  The two paths are in the
// nodePair.
//
// Returns the selection node created.
//
TIntermNode* TIntermediate::addSelection(TIntermTyped* cond, TIntermNodePair nodePair, const TSourceLoc& loc)
{
    //
    // Don't prune the false path for compile-time constants; it's needed
    // for static access analysis.
    //

    TIntermSelection* node = new TIntermSelection(cond, nodePair.node1, nodePair.node2);
    node->setLoc(loc);

    return node;
}


TIntermTyped* TIntermediate::addComma(TIntermTyped* left, TIntermTyped* right, const TSourceLoc& loc)
{
    // However, the lowest precedence operators of the sequence operator ( , ) and the assignment operators
    // ... are not included in the operators that can create a constant expression.
    //
    //if (left->getType().getQualifier().storage == EvqConst &&
    //    right->getType().getQualifier().storage == EvqConst) {

    //    return right;
    //}

    TIntermTyped *commaAggregate = growAggregate(left, right, loc);
    commaAggregate->getAsAggregate()->setOperator(EOpComma);
    commaAggregate->setType(right->getType());
    commaAggregate->getWritableType().getQualifier().makeTemporary();

    return commaAggregate;
}

TIntermTyped* TIntermediate::addMethod(TIntermTyped* object, const TType& type, const TString* name, const TSourceLoc& loc)
{
    TIntermMethod* method = new TIntermMethod(object, type, *name);
    method->setLoc(loc);

    return method;
}

//
// For "?:" test nodes.  There are three children; a condition,
// a true path, and a false path.  The two paths are specified
// as separate parameters.
//
// Returns the selection node created, or 0 if one could not be.
//
TIntermTyped* TIntermediate::addSelection(TIntermTyped* cond, TIntermTyped* trueBlock, TIntermTyped* falseBlock, const TSourceLoc& loc)
{
    //
    // Get compatible types.
    //
    TIntermTyped* child = addConversion(EOpSequence, trueBlock->getType(), falseBlock);
    if (child)
        falseBlock = child;
    else {
        child = addConversion(EOpSequence, falseBlock->getType(), trueBlock);
        if (child)
            trueBlock = child;
        else
            return 0;
    }

    // After conversion, types have to match.
    if (falseBlock->getType() != trueBlock->getType())
        return 0;

    //
    // See if all the operands are constant, then fold it otherwise not.
    //

    if (cond->getAsConstantUnion() && trueBlock->getAsConstantUnion() && falseBlock->getAsConstantUnion()) {
        if (cond->getAsConstantUnion()->getConstArray()[0].getBConst())
            return trueBlock;
        else
            return falseBlock;
    }

    //
    // Make a selection node.
    //
    TIntermSelection* node = new TIntermSelection(cond, trueBlock, falseBlock, trueBlock->getType());
    node->getQualifier().makeTemporary();
    node->setLoc(loc);
    node->getQualifier().precision = std::max(trueBlock->getQualifier().precision, falseBlock->getQualifier().precision);

    return node;
}

//
// Constant terminal nodes.  Has a union that contains bool, float or int constants
//
// Returns the constant union node created.
//

TIntermConstantUnion* TIntermediate::addConstantUnion(const TConstUnionArray& unionArray, const TType& t, const TSourceLoc& loc, bool literal) const
{
    TIntermConstantUnion* node = new TIntermConstantUnion(unionArray, t);
    node->setLoc(loc);
    if (literal)
        node->setLiteral();

    return node;
}

TIntermConstantUnion* TIntermediate::addConstantUnion(int i, const TSourceLoc& loc, bool literal) const
{
    TConstUnionArray unionArray(1);
    unionArray[0].setIConst(i);

    return addConstantUnion(unionArray, TType(EbtInt, EvqConst), loc, literal);
}

TIntermConstantUnion* TIntermediate::addConstantUnion(unsigned int u, const TSourceLoc& loc, bool literal) const
{
    TConstUnionArray unionArray(1);
    unionArray[0].setUConst(u);

    return addConstantUnion(unionArray, TType(EbtUint, EvqConst), loc, literal);
}

TIntermConstantUnion* TIntermediate::addConstantUnion(long long i64, const TSourceLoc& loc, bool literal) const
{
    TConstUnionArray unionArray(1);
    unionArray[0].setI64Const(i64);

    return addConstantUnion(unionArray, TType(EbtInt64, EvqConst), loc, literal);
}

TIntermConstantUnion* TIntermediate::addConstantUnion(unsigned long long u64, const TSourceLoc& loc, bool literal) const
{
    TConstUnionArray unionArray(1);
    unionArray[0].setU64Const(u64);

    return addConstantUnion(unionArray, TType(EbtUint64, EvqConst), loc, literal);
}

TIntermConstantUnion* TIntermediate::addConstantUnion(bool b, const TSourceLoc& loc, bool literal) const
{
    TConstUnionArray unionArray(1);
    unionArray[0].setBConst(b);

    return addConstantUnion(unionArray, TType(EbtBool, EvqConst), loc, literal);
}

TIntermConstantUnion* TIntermediate::addConstantUnion(double d, TBasicType baseType, const TSourceLoc& loc, bool literal) const
{
    assert(baseType == EbtFloat || baseType == EbtDouble);

    TConstUnionArray unionArray(1);
    unionArray[0].setDConst(d);

    return addConstantUnion(unionArray, TType(baseType, EvqConst), loc, literal);
}

TIntermTyped* TIntermediate::addSwizzle(TVectorFields& fields, const TSourceLoc& loc)
{
    TIntermAggregate* node = new TIntermAggregate(EOpSequence);

    node->setLoc(loc);
    TIntermConstantUnion* constIntNode;
    TIntermSequence &sequenceVector = node->getSequence();

    for (int i = 0; i < fields.num; i++) {
        constIntNode = addConstantUnion(fields.offsets[i], loc);
        sequenceVector.push_back(constIntNode);
    }

    return node;
}

//
// Follow the left branches down to the root of an l-value
// expression (just "." and []).
//
// Return the base of the l-value (where following indexing quits working).
// Return nullptr if a chain following dereferences cannot be followed.
//
// 'swizzleOkay' says whether or not it is okay to consider a swizzle
// a valid part of the dereference chain.
//
const TIntermTyped* TIntermediate::findLValueBase(const TIntermTyped* node, bool swizzleOkay)
{
    do {
        const TIntermBinary* binary = node->getAsBinaryNode();
        if (binary == nullptr)
            return node;
        TOperator op = binary->getOp();
        if (op != EOpIndexDirect && op != EOpIndexIndirect && op != EOpIndexDirectStruct && op != EOpVectorSwizzle)
            return nullptr;
        if (! swizzleOkay) {
            if (op == EOpVectorSwizzle)
                return nullptr;
            if ((op == EOpIndexDirect || op == EOpIndexIndirect) &&
                (binary->getLeft()->getType().isVector() || binary->getLeft()->getType().isScalar()) &&
                ! binary->getLeft()->getType().isArray())
                return nullptr;
        }
        node = node->getAsBinaryNode()->getLeft();
    } while (true);
}

//
// Create loop nodes.
//
TIntermLoop* TIntermediate::addLoop(TIntermNode* body, TIntermTyped* test, TIntermTyped* terminal, bool testFirst, const TSourceLoc& loc)
{
    TIntermLoop* node = new TIntermLoop(body, test, terminal, testFirst);
    node->setLoc(loc);

    return node;
}

//
// Add branches.
//
TIntermBranch* TIntermediate::addBranch(TOperator branchOp, const TSourceLoc& loc)
{
    return addBranch(branchOp, 0, loc);
}

TIntermBranch* TIntermediate::addBranch(TOperator branchOp, TIntermTyped* expression, const TSourceLoc& loc)
{
    TIntermBranch* node = new TIntermBranch(branchOp, expression);
    node->setLoc(loc);

    return node;
}

//
// This is to be executed after the final root is put on top by the parsing
// process.
//
bool TIntermediate::postProcess(TIntermNode* root, EShLanguage /*language*/)
{
    if (root == 0)
        return true;

    // Finish off the top-level sequence
    TIntermAggregate* aggRoot = root->getAsAggregate();
    if (aggRoot && aggRoot->getOp() == EOpNull)
        aggRoot->setOperator(EOpSequence);

    // Propagate 'noContraction' label in backward from 'precise' variables.
    glslang::PropagateNoContraction(*this);

    return true;
}

void TIntermediate::addSymbolLinkageNodes(TIntermAggregate*& linkage, EShLanguage language, TSymbolTable& symbolTable)
{
    // Add top-level nodes for declarations that must be checked cross
    // compilation unit by a linker, yet might not have been referenced
    // by the AST.
    //
    // Almost entirely, translation of symbols is driven by what's present
    // in the AST traversal, not by translating the symbol table.
    //
    // However, there are some special cases:
    //  - From the specification: "Special built-in inputs gl_VertexID and
    //    gl_InstanceID are also considered active vertex attributes."
    //  - Linker-based type mismatch error reporting needs to see all
    //    uniforms/ins/outs variables and blocks.
    //  - ftransform() can make gl_Vertex and gl_ModelViewProjectionMatrix active.
    //

    //if (ftransformUsed) {
        // TODO: 1.1 lowering functionality: track ftransform() usage
    //    addSymbolLinkageNode(root, symbolTable, "gl_Vertex");
    //    addSymbolLinkageNode(root, symbolTable, "gl_ModelViewProjectionMatrix");
    //}

    if (language == EShLangVertex) {
        // the names won't be found in the symbol table unless the versions are right,
        // so version logic does not need to be repeated here
        addSymbolLinkageNode(linkage, symbolTable, "gl_VertexID");
        addSymbolLinkageNode(linkage, symbolTable, "gl_InstanceID");
    }

    // Add a child to the root node for the linker objects
    linkage->setOperator(EOpLinkerObjects);
    treeRoot = growAggregate(treeRoot, linkage);
}

//
// Add the given name or symbol to the list of nodes at the end of the tree used
// for link-time checking and external linkage.
//

void TIntermediate::addSymbolLinkageNode(TIntermAggregate*& linkage, TSymbolTable& symbolTable, const TString& name)
{
    TSymbol* symbol = symbolTable.find(name);
    if (symbol)
        addSymbolLinkageNode(linkage, *symbol->getAsVariable());
}

void TIntermediate::addSymbolLinkageNode(TIntermAggregate*& linkage, const TSymbol& symbol)
{
    const TVariable* variable = symbol.getAsVariable();
    if (! variable) {
        // This must be a member of an anonymous block, and we need to add the whole block
        const TAnonMember* anon = symbol.getAsAnonMember();
        variable = &anon->getAnonContainer();
    }
    TIntermSymbol* node = addSymbol(*variable);
    linkage = growAggregate(linkage, node);
}

//
// Add a caller->callee relationship to the call graph.
// Assumes the strings are unique per signature.
//
void TIntermediate::addToCallGraph(TInfoSink& /*infoSink*/, const TString& caller, const TString& callee)
{
    // Duplicates are okay, but faster to not keep them, and they come grouped by caller,
    // as long as new ones are push on the same end we check on for duplicates
    for (TGraph::const_iterator call = callGraph.begin(); call != callGraph.end(); ++call) {
        if (call->caller != caller)
            break;
        if (call->callee == callee)
            return;
    }

    callGraph.push_front(TCall(caller, callee));
}

//
// This deletes the tree.
//
void TIntermediate::removeTree()
{
    if (treeRoot)
        RemoveAllTreeNodes(treeRoot);
}

//
// Implement the part of KHR_vulkan_glsl that lists the set of operations
// that can result in a specialization constant operation.
//
// "5.x Specialization Constant Operations"
//
// ...
//
// It also needs to allow basic construction, swizzling, and indexing
// operations.
//
bool TIntermediate::isSpecializationOperation(const TIntermOperator& node) const
{
    // allow construction
    if (node.isConstructor())
        return true;

    // The set for floating point is quite limited
    if (node.getBasicType() == EbtFloat ||
        node.getBasicType() == EbtDouble) {
        switch (node.getOp()) {
        case EOpIndexDirect:
        case EOpIndexIndirect:
        case EOpIndexDirectStruct:
        case EOpVectorSwizzle:
            return true;
        default:
            return false;
        }
    }

    // Floating-point is out of the way.
    // Now check for integer/bool-based operations
    switch (node.getOp()) {

    // dereference/swizzle
    case EOpIndexDirect:
    case EOpIndexIndirect:
    case EOpIndexDirectStruct:
    case EOpVectorSwizzle:

    // conversion constructors
    case EOpConvIntToBool:
    case EOpConvUintToBool:
    case EOpConvUintToInt:
    case EOpConvBoolToInt:
    case EOpConvIntToUint:
    case EOpConvBoolToUint:

    // unary operations
    case EOpNegative:
    case EOpLogicalNot:
    case EOpBitwiseNot:

    // binary operations
    case EOpAdd:
    case EOpSub:
    case EOpMul:
    case EOpVectorTimesScalar:
    case EOpDiv:
    case EOpMod:
    case EOpRightShift:
    case EOpLeftShift:
    case EOpAnd:
    case EOpInclusiveOr:
    case EOpExclusiveOr:
    case EOpLogicalOr:
    case EOpLogicalXor:
    case EOpLogicalAnd:
    case EOpEqual:
    case EOpNotEqual:
    case EOpLessThan:
    case EOpGreaterThan:
    case EOpLessThanEqual:
    case EOpGreaterThanEqual:
        return true;
    default:
        return false;
    }
}

////////////////////////////////////////////////////////////////
//
// Member functions of the nodes used for building the tree.
//
////////////////////////////////////////////////////////////////

//
// Say whether or not an operation node changes the value of a variable.
//
// Returns true if state is modified.
//
bool TIntermOperator::modifiesState() const
{
    switch (op) {
    case EOpPostIncrement:
    case EOpPostDecrement:
    case EOpPreIncrement:
    case EOpPreDecrement:
    case EOpAssign:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpMulAssign:
    case EOpVectorTimesMatrixAssign:
    case EOpVectorTimesScalarAssign:
    case EOpMatrixTimesScalarAssign:
    case EOpMatrixTimesMatrixAssign:
    case EOpDivAssign:
    case EOpModAssign:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:
    case EOpLeftShiftAssign:
    case EOpRightShiftAssign:
        return true;
    default:
        return false;
    }
}

//
// returns true if the operator is for one of the constructors
//
bool TIntermOperator::isConstructor() const
{
    return op > EOpConstructGuardStart && op < EOpConstructGuardEnd;
}

//
// Make sure the type of a unary operator is appropriate for its
// combination of operation and operand type.
//
// Returns false in nothing makes sense.
//
bool TIntermUnary::promote()
{
    switch (op) {
    case EOpLogicalNot:
        if (operand->getBasicType() != EbtBool)

            return false;
        break;
    case EOpBitwiseNot:
        if (operand->getBasicType() != EbtInt &&
            operand->getBasicType() != EbtUint &&
            operand->getBasicType() != EbtInt64 &&
            operand->getBasicType() != EbtUint64)

            return false;
        break;
    case EOpNegative:
    case EOpPostIncrement:
    case EOpPostDecrement:
    case EOpPreIncrement:
    case EOpPreDecrement:
        if (operand->getBasicType() != EbtInt &&
            operand->getBasicType() != EbtUint &&
            operand->getBasicType() != EbtInt64 &&
            operand->getBasicType() != EbtUint64 &&
            operand->getBasicType() != EbtFloat &&
            operand->getBasicType() != EbtDouble)

            return false;
        break;

    default:
        if (operand->getBasicType() != EbtFloat)

            return false;
    }

    setType(operand->getType());
    getWritableType().getQualifier().makeTemporary();

    return true;
}

void TIntermUnary::updatePrecision()
{
    if (getBasicType() == EbtInt || getBasicType() == EbtUint || getBasicType() == EbtFloat) {
        if (operand->getQualifier().precision > getQualifier().precision)
            getQualifier().precision = operand->getQualifier().precision;
    }
}

//
// Establishes the type of the resultant operation, as well as
// makes the operator the correct one for the operands.
//
// Returns false if operator can't work on operands.
//
bool TIntermBinary::promote()
{
    // Arrays and structures have to be exact matches.
    if ((left->isArray() || right->isArray() || left->getBasicType() == EbtStruct || right->getBasicType() == EbtStruct)
        && left->getType() != right->getType())
        return false;

    // Base assumption:  just make the type the same as the left
    // operand.  Only deviations from this will be coded.
    setType(left->getType());
    type.getQualifier().clear();

    // Composite and opaque types don't having pending operator changes, e.g.,
    // array, structure, and samplers.  Just establish final type and correctness.
    if (left->isArray() || left->getBasicType() == EbtStruct || left->getBasicType() == EbtSampler) {
        switch (op) {
        case EOpEqual:
        case EOpNotEqual:
            if (left->getBasicType() == EbtSampler) {
                // can't compare samplers
                return false;
            } else {
                // Promote to conditional
                setType(TType(EbtBool));
            }

            return true;

        case EOpAssign:
            // Keep type from above

            return true;

        default:
            return false;
        }
    }

    //
    // We now have only scalars, vectors, and matrices to worry about.
    //

    // Do general type checks against individual operands (comparing left and right is coming up, checking mixed shapes after that)
    switch (op) {
    case EOpLessThan:
    case EOpGreaterThan:
    case EOpLessThanEqual:
    case EOpGreaterThanEqual:
        // Relational comparisons need matching numeric types and will promote to scalar Boolean.
        if (left->getBasicType() == EbtBool || left->getType().isVector() || left->getType().isMatrix())
            return false;

        // Fall through

    case EOpEqual:
    case EOpNotEqual:
        // All the above comparisons result in a bool (but not the vector compares)
        setType(TType(EbtBool));
        break;

    case EOpLogicalAnd:
    case EOpLogicalOr:
    case EOpLogicalXor:
        // logical ops operate only on scalar Booleans and will promote to scalar Boolean.
        if (left->getBasicType() != EbtBool || left->isVector() || left->isMatrix())
            return false;

        setType(TType(EbtBool));
        break;

    case EOpRightShift:
    case EOpLeftShift:
    case EOpRightShiftAssign:
    case EOpLeftShiftAssign:

    case EOpMod:
    case EOpModAssign:

    case EOpAnd:
    case EOpInclusiveOr:
    case EOpExclusiveOr:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:
        // Check for integer-only operands.
        if ((left->getBasicType() != EbtInt &&  left->getBasicType() != EbtUint &&
             left->getBasicType() != EbtInt64 &&  left->getBasicType() != EbtUint64) ||
            (right->getBasicType() != EbtInt && right->getBasicType() != EbtUint &&
             right->getBasicType() != EbtInt64 && right->getBasicType() != EbtUint64))
            return false;
        if (left->isMatrix() || right->isMatrix())
            return false;

        break;

    case EOpAdd:
    case EOpSub:
    case EOpDiv:
    case EOpMul:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpMulAssign:
    case EOpDivAssign:
        // check for non-Boolean operands
        if (left->getBasicType() == EbtBool || right->getBasicType() == EbtBool)
            return false;

    default:
        break;
    }

    // Compare left and right, and finish with the cases where the operand types must match
    switch (op) {
    case EOpLessThan:
    case EOpGreaterThan:
    case EOpLessThanEqual:
    case EOpGreaterThanEqual:

    case EOpEqual:
    case EOpNotEqual:

    case EOpLogicalAnd:
    case EOpLogicalOr:
    case EOpLogicalXor:
        return left->getType() == right->getType();

    // no shifts: they can mix types (scalar int can shift a vector uint, etc.)

    case EOpMod:
    case EOpModAssign:

    case EOpAnd:
    case EOpInclusiveOr:
    case EOpExclusiveOr:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:

    case EOpAdd:
    case EOpSub:
    case EOpDiv:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpDivAssign:
        // Quick out in case the types do match
        if (left->getType() == right->getType())
            return true;

        // Fall through

    case EOpMul:
    case EOpMulAssign:
        // At least the basic type has to match
        if (left->getBasicType() != right->getBasicType())
            return false;

    default:
        break;
    }

    // Finish handling the case, for all ops, where both operands are scalars.
    if (left->isScalar() && right->isScalar())
        return true;

    // Finish handling the case, for all ops, where there are two vectors of different sizes
    if (left->isVector() && right->isVector() && left->getVectorSize() != right->getVectorSize())
        return false;

    //
    // We now have a mix of scalars, vectors, or matrices, for non-relational operations.
    //

    // Can these two operands be combined, what is the resulting type?
    TBasicType basicType = left->getBasicType();
    switch (op) {
    case EOpMul:
        if (!left->isMatrix() && right->isMatrix()) {
            if (left->isVector()) {
                if (left->getVectorSize() != right->getMatrixRows())
                    return false;
                op = EOpVectorTimesMatrix;
                setType(TType(basicType, EvqTemporary, right->getMatrixCols()));
            } else {
                op = EOpMatrixTimesScalar;
                setType(TType(basicType, EvqTemporary, 0, right->getMatrixCols(), right->getMatrixRows()));
            }
        } else if (left->isMatrix() && !right->isMatrix()) {
            if (right->isVector()) {
                if (left->getMatrixCols() != right->getVectorSize())
                    return false;
                op = EOpMatrixTimesVector;
                setType(TType(basicType, EvqTemporary, left->getMatrixRows()));
            } else {
                op = EOpMatrixTimesScalar;
            }
        } else if (left->isMatrix() && right->isMatrix()) {
            if (left->getMatrixCols() != right->getMatrixRows())
                return false;
            op = EOpMatrixTimesMatrix;
            setType(TType(basicType, EvqTemporary, 0, right->getMatrixCols(), left->getMatrixRows()));
        } else if (! left->isMatrix() && ! right->isMatrix()) {
            if (left->isVector() && right->isVector()) {
                ; // leave as component product
            } else if (left->isVector() || right->isVector()) {
                op = EOpVectorTimesScalar;
                if (right->isVector())
                    setType(TType(basicType, EvqTemporary, right->getVectorSize()));
            }
        } else {
            return false;
        }
        break;
    case EOpMulAssign:
        if (! left->isMatrix() && right->isMatrix()) {
            if (left->isVector()) {
                if (left->getVectorSize() != right->getMatrixRows() || left->getVectorSize() != right->getMatrixCols())
                    return false;
                op = EOpVectorTimesMatrixAssign;
            } else {
                return false;
            }
        } else if (left->isMatrix() && !right->isMatrix()) {
            if (right->isVector()) {
                return false;
            } else {
                op = EOpMatrixTimesScalarAssign;
            }
        } else if (left->isMatrix() && right->isMatrix()) {
            if (left->getMatrixCols() != left->getMatrixRows() || left->getMatrixCols() != right->getMatrixCols() || left->getMatrixCols() != right->getMatrixRows())
                return false;
            op = EOpMatrixTimesMatrixAssign;
        } else if (!left->isMatrix() && !right->isMatrix()) {
            if (left->isVector() && right->isVector()) {
                // leave as component product
            } else if (left->isVector() || right->isVector()) {
                if (! left->isVector())
                    return false;
                op = EOpVectorTimesScalarAssign;
            }
        } else {
            return false;
        }
        break;

    case EOpRightShift:
    case EOpLeftShift:
    case EOpRightShiftAssign:
    case EOpLeftShiftAssign:
        if (right->isVector() && (! left->isVector() || right->getVectorSize() != left->getVectorSize()))
            return false;
        break;

    case EOpAssign:
        if (left->getVectorSize() != right->getVectorSize() || left->getMatrixCols() != right->getMatrixCols() || left->getMatrixRows() != right->getMatrixRows())
            return false;
        // fall through

    case EOpAdd:
    case EOpSub:
    case EOpDiv:
    case EOpMod:
    case EOpAnd:
    case EOpInclusiveOr:
    case EOpExclusiveOr:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpDivAssign:
    case EOpModAssign:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:
        if ((left->isMatrix() && right->isVector()) ||
            (left->isVector() && right->isMatrix()) ||
            left->getBasicType() != right->getBasicType())
            return false;
        if (left->isMatrix() && right->isMatrix() && (left->getMatrixCols() != right->getMatrixCols() || left->getMatrixRows() != right->getMatrixRows()))
            return false;
        if (left->isVector() && right->isVector() && left->getVectorSize() != right->getVectorSize())
            return false;
        if (right->isVector() || right->isMatrix())
            setType(TType(basicType, EvqTemporary, right->getVectorSize(), right->getMatrixCols(), right->getMatrixRows()));
        break;

    default:
        return false;
    }

    //
    // One more check for assignment.
    //
    switch (op) {
    // The resulting type has to match the left operand.
    case EOpAssign:
    case EOpAddAssign:
    case EOpSubAssign:
    case EOpMulAssign:
    case EOpDivAssign:
    case EOpModAssign:
    case EOpAndAssign:
    case EOpInclusiveOrAssign:
    case EOpExclusiveOrAssign:
    case EOpLeftShiftAssign:
    case EOpRightShiftAssign:
        if (getType() != left->getType())
            return false;
        break;
    default:
        break;
    }

    return true;
}

void TIntermBinary::updatePrecision()
{
    if (getBasicType() == EbtInt || getBasicType() == EbtUint || getBasicType() == EbtFloat) {
        getQualifier().precision = std::max(right->getQualifier().precision, left->getQualifier().precision);
        if (getQualifier().precision != EpqNone) {
            left->propagatePrecision(getQualifier().precision);
            right->propagatePrecision(getQualifier().precision);
        }
    }
}

void TIntermTyped::propagatePrecision(TPrecisionQualifier newPrecision)
{
    if (getQualifier().precision != EpqNone || (getBasicType() != EbtInt && getBasicType() != EbtUint && getBasicType() != EbtFloat))
        return;

    getQualifier().precision = newPrecision;

    TIntermBinary* binaryNode = getAsBinaryNode();
    if (binaryNode) {
        binaryNode->getLeft()->propagatePrecision(newPrecision);
        binaryNode->getRight()->propagatePrecision(newPrecision);

        return;
    }

    TIntermUnary* unaryNode = getAsUnaryNode();
    if (unaryNode) {
        unaryNode->getOperand()->propagatePrecision(newPrecision);

        return;
    }

    TIntermAggregate* aggregateNode = getAsAggregate();
    if (aggregateNode) {
        TIntermSequence operands = aggregateNode->getSequence();
        for (unsigned int i = 0; i < operands.size(); ++i) {
            TIntermTyped* typedNode = operands[i]->getAsTyped();
            if (! typedNode)
                break;
            typedNode->propagatePrecision(newPrecision);
        }

        return;
    }

    TIntermSelection* selectionNode = getAsSelectionNode();
    if (selectionNode) {
        TIntermTyped* typedNode = selectionNode->getTrueBlock()->getAsTyped();
        if (typedNode) {
            typedNode->propagatePrecision(newPrecision);
            typedNode = selectionNode->getFalseBlock()->getAsTyped();
            if (typedNode)
                typedNode->propagatePrecision(newPrecision);
        }

        return;
    }
}

TIntermTyped* TIntermediate::promoteConstantUnion(TBasicType promoteTo, TIntermConstantUnion* node) const
{
    const TConstUnionArray& rightUnionArray = node->getConstArray();
    int size = node->getType().computeNumComponents();

    TConstUnionArray leftUnionArray(size);

    for (int i=0; i < size; i++) {
        switch (promoteTo) {
        case EbtFloat:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getIConst()));
                break;
            case EbtUint:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getUConst()));
                break;
            case EbtInt64:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getI64Const()));
                break;
            case EbtUint64:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getU64Const()));
                break;
            case EbtBool:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtDouble:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getDConst()));
                break;
            default:
                return node;
            }
            break;
        case EbtDouble:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getIConst()));
                break;
            case EbtUint:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getUConst()));
                break;
            case EbtInt64:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getI64Const()));
                break;
            case EbtUint64:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getU64Const()));
                break;
            case EbtBool:
                leftUnionArray[i].setDConst(static_cast<double>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            default:
                return node;
            }
            break;
        case EbtInt:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtUint:
                leftUnionArray[i].setIConst(static_cast<int>(rightUnionArray[i].getUConst()));
                break;
            case EbtInt64:
                leftUnionArray[i].setIConst(static_cast<int>(rightUnionArray[i].getI64Const()));
                break;
            case EbtUint64:
                leftUnionArray[i].setIConst(static_cast<int>(rightUnionArray[i].getU64Const()));
                break;
            case EbtBool:
                leftUnionArray[i].setIConst(static_cast<int>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i].setIConst(static_cast<int>(rightUnionArray[i].getDConst()));
                break;
            default:
                return node;
            }
            break;
        case EbtUint:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setUConst(static_cast<unsigned int>(rightUnionArray[i].getIConst()));
                break;
            case EbtUint:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtInt64:
                leftUnionArray[i].setUConst(static_cast<unsigned int>(rightUnionArray[i].getI64Const()));
                break;
            case EbtUint64:
                leftUnionArray[i].setUConst(static_cast<unsigned int>(rightUnionArray[i].getU64Const()));
                break;
            case EbtBool:
                leftUnionArray[i].setUConst(static_cast<unsigned int>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i].setUConst(static_cast<unsigned int>(rightUnionArray[i].getDConst()));
                break;
            default:
                return node;
            }
            break;
        case EbtBool:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setBConst(rightUnionArray[i].getIConst() != 0);
                break;
            case EbtUint:
                leftUnionArray[i].setBConst(rightUnionArray[i].getUConst() != 0);
                break;
            case EbtInt64:
                leftUnionArray[i].setBConst(rightUnionArray[i].getI64Const() != 0);
                break;
            case EbtUint64:
                leftUnionArray[i].setBConst(rightUnionArray[i].getU64Const() != 0);
                break;
            case EbtBool:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i].setBConst(rightUnionArray[i].getDConst() != 0.0);
                break;
            default:
                return node;
            }
            break;
        case EbtInt64:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setI64Const(static_cast<long long>(rightUnionArray[i].getIConst()));
                break;
            case EbtUint:
                leftUnionArray[i].setI64Const(static_cast<long long>(rightUnionArray[i].getUConst()));
                break;
            case EbtInt64:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtUint64:
                leftUnionArray[i].setI64Const(static_cast<long long>(rightUnionArray[i].getU64Const()));
                break;
            case EbtBool:
                leftUnionArray[i].setI64Const(static_cast<long long>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i].setI64Const(static_cast<long long>(rightUnionArray[i].getDConst()));
                break;
            default:
                return node;
            }
            break;
        case EbtUint64:
            switch (node->getType().getBasicType()) {
            case EbtInt:
                leftUnionArray[i].setU64Const(static_cast<unsigned long long>(rightUnionArray[i].getIConst()));
                break;
            case EbtUint:
                leftUnionArray[i].setU64Const(static_cast<unsigned long long>(rightUnionArray[i].getUConst()));
                break;
            case EbtInt64:
                leftUnionArray[i].setU64Const(static_cast<unsigned long long>(rightUnionArray[i].getI64Const()));
                break;
            case EbtUint64:
                leftUnionArray[i] = rightUnionArray[i];
                break;
            case EbtBool:
                leftUnionArray[i].setU64Const(static_cast<unsigned long long>(rightUnionArray[i].getBConst()));
                break;
            case EbtFloat:
            case EbtDouble:
                leftUnionArray[i].setU64Const(static_cast<unsigned long long>(rightUnionArray[i].getDConst()));
                break;
            default:
                return node;
            }
            break;
        default:
            return node;
        }
    }

    const TType& t = node->getType();

    return addConstantUnion(leftUnionArray, TType(promoteTo, t.getQualifier().storage, t.getVectorSize(), t.getMatrixCols(), t.getMatrixRows()),
                            node->getLoc());
}

void TIntermAggregate::addToPragmaTable(const TPragmaTable& pTable)
{
    assert(!pragmaTable);
    pragmaTable = new TPragmaTable();
    *pragmaTable = pTable;
}

} // end namespace glslang
