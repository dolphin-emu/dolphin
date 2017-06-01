////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/BlendMode.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
// Commonly used blending modes
////////////////////////////////////////////////////////////
const BlendMode BlendAlpha(BlendMode::SrcAlpha, BlendMode::OneMinusSrcAlpha, BlendMode::Add,
                           BlendMode::One, BlendMode::OneMinusSrcAlpha, BlendMode::Add);
const BlendMode BlendAdd(BlendMode::SrcAlpha, BlendMode::One, BlendMode::Add,
                         BlendMode::One, BlendMode::One, BlendMode::Add);
const BlendMode BlendMultiply(BlendMode::DstColor, BlendMode::Zero);
const BlendMode BlendNone(BlendMode::One, BlendMode::Zero);


////////////////////////////////////////////////////////////
BlendMode::BlendMode() :
colorSrcFactor(BlendMode::SrcAlpha),
colorDstFactor(BlendMode::OneMinusSrcAlpha),
colorEquation (BlendMode::Add),
alphaSrcFactor(BlendMode::One),
alphaDstFactor(BlendMode::OneMinusSrcAlpha),
alphaEquation (BlendMode::Add)
{

}


////////////////////////////////////////////////////////////
BlendMode::BlendMode(Factor sourceFactor, Factor destinationFactor, Equation blendEquation) :
colorSrcFactor(sourceFactor),
colorDstFactor(destinationFactor),
colorEquation (blendEquation),
alphaSrcFactor(sourceFactor),
alphaDstFactor(destinationFactor),
alphaEquation (blendEquation)
{

}


////////////////////////////////////////////////////////////
BlendMode::BlendMode(Factor colorSourceFactor, Factor colorDestinationFactor,
                     Equation colorBlendEquation, Factor alphaSourceFactor,
                     Factor alphaDestinationFactor, Equation alphaBlendEquation) :
colorSrcFactor(colorSourceFactor),
colorDstFactor(colorDestinationFactor),
colorEquation (colorBlendEquation),
alphaSrcFactor(alphaSourceFactor),
alphaDstFactor(alphaDestinationFactor),
alphaEquation (alphaBlendEquation)
{

}


////////////////////////////////////////////////////////////
bool operator ==(const BlendMode& left, const BlendMode& right)
{
    return (left.colorSrcFactor == right.colorSrcFactor) &&
           (left.colorDstFactor == right.colorDstFactor) &&
           (left.colorEquation  == right.colorEquation)  &&
           (left.alphaSrcFactor == right.alphaSrcFactor) &&
           (left.alphaDstFactor == right.alphaDstFactor) &&
           (left.alphaEquation  == right.alphaEquation);
}


////////////////////////////////////////////////////////////
bool operator !=(const BlendMode& left, const BlendMode& right)
{
    return !(left == right);
}

} // namespace sf
