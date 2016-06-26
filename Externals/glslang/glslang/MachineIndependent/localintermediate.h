//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
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

#ifndef _LOCAL_INTERMEDIATE_INCLUDED_
#define _LOCAL_INTERMEDIATE_INCLUDED_

#include "../Include/intermediate.h"
#include "../Public/ShaderLang.h"
#include "Versions.h"

#include <algorithm>
#include <set>

class TInfoSink;

namespace glslang {

struct TVectorFields {
    int offsets[4];
    int num;
};

//
// Some helper structures for TIntermediate.  Their contents are encapsulated
// by TIntermediate.
//

// Used for detecting recursion:  A "call" is a pair: <caller, callee>.
struct TCall {
    TCall(const TString& pCaller, const TString& pCallee) : caller(pCaller), callee(pCallee) { }
    TString caller;
    TString callee;
    bool visited;
    bool currentPath;
    bool errorGiven;
};

// A generic 1-D range.
struct TRange {
    TRange(int start, int last) : start(start), last(last) { }
    bool overlap(const TRange& rhs) const 
    {
        return last >= rhs.start && start <= rhs.last;
    }
    int start;
    int last;
};

// An IO range is a 3-D rectangle; the set of (location, component, index) triples all lying
// within the same location range, component range, and index value.  Locations don't alias unless
// all other dimensions of their range overlap.
struct TIoRange {
    TIoRange(TRange location, TRange component, TBasicType basicType, int index)
        : location(location), component(component), basicType(basicType), index(index) { }
    bool overlap(const TIoRange& rhs) const
    {
        return location.overlap(rhs.location) && component.overlap(rhs.component) && index == rhs.index;
    }
    TRange location;
    TRange component;
    TBasicType basicType;
    int index;
};

// An offset range is a 2-D rectangle; the set of (binding, offset) pairs all lying
// within the same binding and offset range.
struct TOffsetRange {
    TOffsetRange(TRange binding, TRange offset)
        : binding(binding), offset(offset) { }
    bool overlap(const TOffsetRange& rhs) const
    {
        return binding.overlap(rhs.binding) && offset.overlap(rhs.offset);
    }
    TRange binding;
    TRange offset;
};

// Things that need to be tracked per xfb buffer.
struct TXfbBuffer {
    TXfbBuffer() : stride(TQualifier::layoutXfbStrideEnd), implicitStride(0), containsDouble(false) { }
    std::vector<TRange> ranges;  // byte offsets that have already been assigned
    unsigned int stride;
    unsigned int implicitStride;
    bool containsDouble;
};

class TSymbolTable;
class TSymbol;
class TVariable;

//
// Set of helper functions to help parse and build the tree.
//
class TIntermediate {
public:
    explicit TIntermediate(EShLanguage l, int v = 0, EProfile p = ENoProfile) :
        source(EShSourceNone), language(l), profile(p), version(v), spv(0), treeRoot(0),
        numMains(0), numErrors(0), numPushConstants(0), recursive(false),
        invocations(TQualifier::layoutNotSet), vertices(TQualifier::layoutNotSet), inputPrimitive(ElgNone), outputPrimitive(ElgNone),
        pixelCenterInteger(false), originUpperLeft(false),
        vertexSpacing(EvsNone), vertexOrder(EvoNone), pointMode(false), earlyFragmentTests(false), depthLayout(EldNone), depthReplacing(false), blendEquations(0),
        multiStream(false), xfbMode(false)
    {
        localSize[0] = 1;
        localSize[1] = 1;
        localSize[2] = 1;
        localSizeSpecId[0] = TQualifier::layoutNotSet;
        localSizeSpecId[1] = TQualifier::layoutNotSet;
        localSizeSpecId[2] = TQualifier::layoutNotSet;
        xfbBuffers.resize(TQualifier::layoutXfbBufferEnd);
    }
    void setLimits(const TBuiltInResource& r) { resources = r; }

    bool postProcess(TIntermNode*, EShLanguage);
    void output(TInfoSink&, bool tree);
    void removeTree();

    void setSource(EShSource s) { source = s; }
    EShSource getSource() const { return source; }
    void setEntryPoint(const char* ep) { entryPoint = ep; }
    const std::string& getEntryPoint() const { return entryPoint; }
    void setVersion(int v) { version = v; }
    int getVersion() const { return version; }
    void setProfile(EProfile p) { profile = p; }
    EProfile getProfile() const { return profile; }
    void setSpv(int s) { spv = s; }
    int getSpv() const { return spv; }
    EShLanguage getStage() const { return language; }
    void addRequestedExtension(const char* extension) { requestedExtensions.insert(extension); }
    const std::set<std::string>& getRequestedExtensions() const { return requestedExtensions; }

    void setTreeRoot(TIntermNode* r) { treeRoot = r; }
    TIntermNode* getTreeRoot() const { return treeRoot; }
    void addMainCount() { ++numMains; }
    int getNumMains() const { return numMains; }
    int getNumErrors() const { return numErrors; }
    void addPushConstantCount() { ++numPushConstants; }
    bool isRecursive() const { return recursive; }

    TIntermSymbol* addSymbol(const TVariable&);
    TIntermSymbol* addSymbol(const TVariable&, const TSourceLoc&);
    TIntermSymbol* addSymbol(const TType&, const TSourceLoc&);
    TIntermTyped* addConversion(TOperator, const TType&, TIntermTyped*) const;
    TIntermTyped* addBinaryMath(TOperator, TIntermTyped* left, TIntermTyped* right, TSourceLoc);
    TIntermTyped* addAssign(TOperator op, TIntermTyped* left, TIntermTyped* right, TSourceLoc);
    TIntermTyped* addIndex(TOperator op, TIntermTyped* base, TIntermTyped* index, TSourceLoc);
    TIntermTyped* addUnaryMath(TOperator, TIntermTyped* child, TSourceLoc);
    TIntermTyped* addBuiltInFunctionCall(const TSourceLoc& line, TOperator, bool unary, TIntermNode*, const TType& returnType);
    bool canImplicitlyPromote(TBasicType from, TBasicType to) const;
    TIntermAggregate* growAggregate(TIntermNode* left, TIntermNode* right);
    TIntermAggregate* growAggregate(TIntermNode* left, TIntermNode* right, const TSourceLoc&);
    TIntermAggregate* makeAggregate(TIntermNode* node);
    TIntermAggregate* makeAggregate(TIntermNode* node, const TSourceLoc&);
    TIntermTyped* setAggregateOperator(TIntermNode*, TOperator, const TType& type, TSourceLoc);
    bool areAllChildConst(TIntermAggregate* aggrNode);
    TIntermNode*  addSelection(TIntermTyped* cond, TIntermNodePair code, const TSourceLoc&);
    TIntermTyped* addSelection(TIntermTyped* cond, TIntermTyped* trueBlock, TIntermTyped* falseBlock, const TSourceLoc&);
    TIntermTyped* addComma(TIntermTyped* left, TIntermTyped* right, const TSourceLoc&);
    TIntermTyped* addMethod(TIntermTyped*, const TType&, const TString*, const TSourceLoc&);
    TIntermConstantUnion* addConstantUnion(const TConstUnionArray&, const TType&, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(int, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(unsigned int, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(long long, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(unsigned long long, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(bool, const TSourceLoc&, bool literal = false) const;
    TIntermConstantUnion* addConstantUnion(double, TBasicType, const TSourceLoc&, bool literal = false) const;
    TIntermTyped* promoteConstantUnion(TBasicType, TIntermConstantUnion*) const;
    bool parseConstTree(TIntermNode*, TConstUnionArray, TOperator, const TType&, bool singleConstantParam = false);
    TIntermLoop* addLoop(TIntermNode*, TIntermTyped*, TIntermTyped*, bool testFirst, const TSourceLoc&);
    TIntermBranch* addBranch(TOperator, const TSourceLoc&);
    TIntermBranch* addBranch(TOperator, TIntermTyped*, const TSourceLoc&);
    TIntermTyped* addSwizzle(TVectorFields&, const TSourceLoc&);

    // Constant folding (in Constant.cpp)
    TIntermTyped* fold(TIntermAggregate* aggrNode);
    TIntermTyped* foldConstructor(TIntermAggregate* aggrNode);
    TIntermTyped* foldDereference(TIntermTyped* node, int index, const TSourceLoc&);
    TIntermTyped* foldSwizzle(TIntermTyped* node, TVectorFields& fields, const TSourceLoc&);

    // Tree ops
    static const TIntermTyped* findLValueBase(const TIntermTyped*, bool swizzleOkay);

    // Linkage related
    void addSymbolLinkageNodes(TIntermAggregate*& linkage, EShLanguage, TSymbolTable&);
    void addSymbolLinkageNode(TIntermAggregate*& linkage, TSymbolTable&, const TString&);
    void addSymbolLinkageNode(TIntermAggregate*& linkage, const TSymbol&);

    bool setInvocations(int i) 
    {
        if (invocations != TQualifier::layoutNotSet)
            return invocations == i;
        invocations = i;
        return true;
    }
    int getInvocations() const { return invocations; }
    bool setVertices(int m)
    {
        if (vertices != TQualifier::layoutNotSet)
            return vertices == m;
        vertices = m;
        return true;
    }
    int getVertices() const { return vertices; }
    bool setInputPrimitive(TLayoutGeometry p)
    {
        if (inputPrimitive != ElgNone)
            return inputPrimitive == p;
        inputPrimitive = p;
        return true;
    }
    TLayoutGeometry getInputPrimitive() const { return inputPrimitive; }
    bool setVertexSpacing(TVertexSpacing s)
    {
        if (vertexSpacing != EvsNone)
            return vertexSpacing == s;
        vertexSpacing = s;
        return true;
    }
    TVertexSpacing getVertexSpacing() const { return vertexSpacing; }
    bool setVertexOrder(TVertexOrder o)
    {
        if (vertexOrder != EvoNone)
            return vertexOrder == o;
        vertexOrder = o;
        return true;
    }
    TVertexOrder getVertexOrder() const { return vertexOrder; }
    void setPointMode() { pointMode = true; }
    bool getPointMode() const { return pointMode; }
    
    bool setLocalSize(int dim, int size)
    {
        if (localSize[dim] > 1)
            return size == localSize[dim];
        localSize[dim] = size;
        return true;
    }
    unsigned int getLocalSize(int dim) const { return localSize[dim]; }

    bool setLocalSizeSpecId(int dim, int id)
    {
        if (localSizeSpecId[dim] != TQualifier::layoutNotSet)
            return id == localSizeSpecId[dim];
        localSizeSpecId[dim] = id;
        return true;
    }
    int getLocalSizeSpecId(int dim) const { return localSizeSpecId[dim]; }

    void setXfbMode() { xfbMode = true; }
    bool getXfbMode() const { return xfbMode; }
    void setMultiStream() { multiStream = true; }
    bool isMultiStream() const { return multiStream; }
    bool setOutputPrimitive(TLayoutGeometry p)
    {
        if (outputPrimitive != ElgNone)
            return outputPrimitive == p;
        outputPrimitive = p;
        return true;
    }
    TLayoutGeometry getOutputPrimitive() const { return outputPrimitive; }
    void setOriginUpperLeft() { originUpperLeft = true; }
    bool getOriginUpperLeft() const { return originUpperLeft; }
    void setPixelCenterInteger() { pixelCenterInteger = true; }
    bool getPixelCenterInteger() const { return pixelCenterInteger; }
    void setEarlyFragmentTests() { earlyFragmentTests = true; }
    bool getEarlyFragmentTests() const { return earlyFragmentTests; }
    bool setDepth(TLayoutDepth d)
    {
        if (depthLayout != EldNone)
            return depthLayout == d;
        depthLayout = d;
        return true;
    }
    TLayoutDepth getDepth() const { return depthLayout; }
    void setDepthReplacing() { depthReplacing = true; }
    bool isDepthReplacing() const { return depthReplacing; }

    void addBlendEquation(TBlendEquationShift b) { blendEquations |= (1 << b); }
    unsigned int getBlendEquations() const { return blendEquations; }

    void addToCallGraph(TInfoSink&, const TString& caller, const TString& callee);
    void merge(TInfoSink&, TIntermediate&);
    void finalCheck(TInfoSink&);

    void addIoAccessed(const TString& name) { ioAccessed.insert(name); }
    bool inIoAccessed(const TString& name) const { return ioAccessed.find(name) != ioAccessed.end(); }

    int addUsedLocation(const TQualifier&, const TType&, bool& typeCollision);
    int addUsedOffsets(int binding, int offset, int numOffsets);
    bool addUsedConstantId(int id);
    int computeTypeLocationSize(const TType&) const;

    bool setXfbBufferStride(int buffer, unsigned stride)
    {
        if (xfbBuffers[buffer].stride != TQualifier::layoutXfbStrideEnd)
            return xfbBuffers[buffer].stride == stride;
        xfbBuffers[buffer].stride = stride;
        return true;
    }
    int addXfbBufferOffset(const TType&);
    unsigned int computeTypeXfbSize(const TType&, bool& containsDouble) const;
    static int getBaseAlignment(const TType&, int& size, int& stride, bool std140, bool rowMajor);

protected:
    TIntermSymbol* addSymbol(int Id, const TString&, const TType&, const TConstUnionArray&, TIntermTyped* subtree, const TSourceLoc&);
    void error(TInfoSink& infoSink, const char*);
    void mergeBodies(TInfoSink&, TIntermSequence& globals, const TIntermSequence& unitGlobals);
    void mergeLinkerObjects(TInfoSink&, TIntermSequence& linkerObjects, const TIntermSequence& unitLinkerObjects);
    void mergeImplicitArraySizes(TType&, const TType&);
    void mergeErrorCheck(TInfoSink&, const TIntermSymbol&, const TIntermSymbol&, bool crossStage);
    void checkCallGraphCycles(TInfoSink&);
    void inOutLocationCheck(TInfoSink&);
    TIntermSequence& findLinkerObjects() const;
    bool userOutputUsed() const;
    static int getBaseAlignmentScalar(const TType&, int& size);
    bool isSpecializationOperation(const TIntermOperator&) const;


    const EShLanguage language;  // stage, known at construction time
    EShSource source;            // source language, known a bit later
    std::string entryPoint;
    EProfile profile;
    int version;
    int spv;
    TIntermNode* treeRoot;
    std::set<std::string> requestedExtensions;  // cumulation of all enabled or required extensions; not connected to what subset of the shader used them
    TBuiltInResource resources;
    int numMains;
    int numErrors;
    int numPushConstants;
    bool recursive;
    int invocations;
    int vertices;
    TLayoutGeometry inputPrimitive;
    TLayoutGeometry outputPrimitive;
    bool pixelCenterInteger;
    bool originUpperLeft;
    TVertexSpacing vertexSpacing;
    TVertexOrder vertexOrder;
    bool pointMode;
    int localSize[3];
    int localSizeSpecId[3];
    bool earlyFragmentTests;
    TLayoutDepth depthLayout;
    bool depthReplacing;
    int blendEquations;        // an 'or'ing of masks of shifts of TBlendEquationShift
    bool xfbMode;
    bool multiStream;

    typedef std::list<TCall> TGraph;
    TGraph callGraph;

    std::set<TString> ioAccessed;           // set of names of statically read/written I/O that might need extra checking
    std::vector<TIoRange> usedIo[4];        // sets of used locations, one for each of in, out, uniform, and buffers
    std::vector<TOffsetRange> usedAtomics;  // sets of bindings used by atomic counters
    std::vector<TXfbBuffer> xfbBuffers;     // all the data we need to track per xfb buffer
    std::unordered_set<int> usedConstantId; // specialization constant ids used

private:
    void operator=(TIntermediate&); // prevent assignments
};

} // end namespace glslang

#endif // _LOCAL_INTERMEDIATE_INCLUDED_
