/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

Copyright (c) 2000-2009 Torus Knot Software Ltd
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#include "OgreShaderExTextureAtlasSampler.h"

#ifdef RTSHADER_SYSTEM_BUILD_EXT_SHADERS
#include "OgreShaderFFPRenderState.h"
#include "OgreShaderProgram.h"
#include "OgreShaderParameter.h"
#include "OgreShaderProgramSet.h"

#define SGX_LIB_TEXTURE_ATLAS "SGXLib_TextureAtlas"

#define SGX_FUNC_ATLAS_SAMPLE "SGX_Atlas_Sample"

#define SGX_FUNC_ATLAS_WRAP "SGX_Atlas_Wrap"
#define SGX_FUNC_ATLAS_MIRROR "SGX_Atlas_Mirror"
#define SGX_FUNC_ATLAS_CLAMP "SGX_Atlas_Clamp"
#define SGX_FUNC_ATLAS_BORDER "SGX_Atlas_Border"

namespace Ogre {
template<> RTShader::TextureAtlasSamplerFactory* Singleton<RTShader::TextureAtlasSamplerFactory>::ms_Singleton = 0;

namespace RTShader {



const TextureAtlasTablePtr c_BlankAtlasTable;
const String c_ParamTexel("texel_");
String TextureAtlasSampler::Type = "SGX_TextureAtlasSampler";

//-----------------------------------------------------------------------
TextureAtlasSampler::TextureAtlasSampler() :
	mAtlasTexcoordPos(0),
	mAtlasTextureStart(0),
	mAtlasTextureCount(0),
	mIsTableDataUpdated(false)
{
}

//-----------------------------------------------------------------------
const String& TextureAtlasSampler::getType() const
{
	return Type;
}


//-----------------------------------------------------------------------
int	TextureAtlasSampler::getExecutionOrder() const
{
	return FFP_TEXTURING + 25;
}

//-----------------------------------------------------------------------
bool TextureAtlasSampler::resolveParameters(ProgramSet* programSet)
{
	Program* vsProgram = programSet->getCpuVertexProgram();
	Program* psProgram = programSet->getCpuFragmentProgram();
	Function* vsMain   = vsProgram->getEntryPointFunction();
	Function* psMain   = psProgram->getEntryPointFunction();	


	//
	// Define vertex shader parameters used to find the position of the textures in the atlas
	//
	Parameter::Content indexContent = (Parameter::Content)((int)Parameter::SPC_TEXTURE_COORDINATE0 + mAtlasTexcoordPos);
	GpuConstantType indexType = (GpuConstantType)((ushort)GCT_FLOAT1 + std::min<ushort>(mAtlasTextureCount,4) - 1);

	mVSInpTextureTableIndex = vsMain->resolveInputParameter(Parameter::SPS_TEXTURE_COORDINATES, 
				mAtlasTexcoordPos, indexContent, indexType);
		
	mVSTextureTable = vsProgram->resolveParameter(GCT_FLOAT4, -1, (uint16)GPV_GLOBAL, "AtlasData", mAtlasTableData->size());
	
	//
	// Define parameters to carry the information on the location of the texture from the vertex to
	// the pixel shader
	//
	for(ushort i = 0 ; i < mAtlasTextureCount ; ++ i)
	{
		mVSOutTextureDatas[i] = vsMain->resolveOutputParameter(Parameter::SPS_TEXTURE_COORDINATES,
				-1, Parameter::SPC_UNKNOWN, GCT_FLOAT4);
		mPSInpTextureDatas[i] = psMain->resolveInputParameter(Parameter::SPS_TEXTURE_COORDINATES, 
			mVSOutTextureDatas[i]->getIndex(), Parameter::SPC_UNKNOWN, GCT_FLOAT4);
	}
	return true;
}


//-----------------------------------------------------------------------
bool TextureAtlasSampler::resolveDependencies(ProgramSet* programSet)
{
	Program* vsProgram = programSet->getCpuVertexProgram();
	Program* psProgram = programSet->getCpuFragmentProgram();
	vsProgram->addDependency(FFP_LIB_COMMON);
	psProgram->addDependency(SGX_LIB_TEXTURE_ATLAS);

	return true;
}

//-----------------------------------------------------------------------
bool TextureAtlasSampler::addFunctionInvocations(ProgramSet* programSet)
{
	Program* vsProgram = programSet->getCpuVertexProgram();
	Function* vsMain   = vsProgram->getEntryPointFunction();	
	Program* psProgram = programSet->getCpuFragmentProgram();
	Function* psMain   = psProgram->getEntryPointFunction();	
	FunctionInvocation* curFuncInvocation = NULL;	

	//
	// Calculate the position and size of the texture in the atlas in the vertex shader
	//
	int groupOrder = (FFP_VS_TEXTURING - FFP_VS_LIGHTING) / 2;
	int internalCounter = 0;

	for(ushort i = 0 ; i < mAtlasTextureCount ; ++i)
	{
		Operand::OpMask textureIndexMask = Operand::OPM_X;
		switch (i)
		{
		case 1: textureIndexMask = Operand::OPM_Y; break;
		case 2: textureIndexMask = Operand::OPM_Z; break;
		case 3: textureIndexMask = Operand::OPM_W; break;
		}
		
		curFuncInvocation = OGRE_NEW FunctionInvocation(FFP_FUNC_ASSIGN, groupOrder, internalCounter++);
		curFuncInvocation->pushOperand(mVSTextureTable, Operand::OPS_IN);
		curFuncInvocation->pushOperand(mVSInpTextureTableIndex, Operand::OPS_IN, textureIndexMask, 1);
		curFuncInvocation->pushOperand(mVSOutTextureDatas[i], Operand::OPS_OUT);
		vsMain->addAtomInstance(curFuncInvocation);
	}

	//
	// sample the texture in the fragment shader given the extracted data in the pixel shader
	//


	groupOrder = (FFP_PS_SAMPLING + FFP_PS_TEXTURING) / 2;

	internalCounter = 0;

	const ShaderParameterList& inpParams = psMain->getInputParameters();
	const ShaderParameterList& localParams = psMain->getLocalParameters();
	
	ParameterPtr psAtlasTextureCoord = psMain->resolveLocalParameter(Parameter::SPS_UNKNOWN, 
		-1, "atlasCoord", GCT_FLOAT2);

	for(ushort j = 0 ; j < mAtlasTextureCount ; ++j)
	{
		int texNum = mAtlasTextureStart + j;
		
		//Find the texture coordinates texel and sampler from the original FFPTexturing
		ParameterPtr texcoord = psMain->getParameterBySemantic(inpParams, Parameter::SPS_TEXTURE_COORDINATES, texNum);
		ParameterPtr texel = psMain->getParameterByName(localParams, c_ParamTexel + Ogre::StringConverter::toString(texNum));
		UniformParameterPtr sampler = psProgram->getParameterByType(GCT_SAMPLER2D, texNum);
			
		const char* addressUFuncName = getAdressingFunctionName(mTextureAddressings[j].u);
		const char* addressVFuncName = getAdressingFunctionName(mTextureAddressings[j].v);
		
		//Create a function which will replace the texel with the texture texel
		if ((texcoord.isNull() == false) && (texel.isNull() == false) && 
			(sampler.isNull() == false) && (addressUFuncName != NULL) && (addressVFuncName != NULL))
		{
			//calculate the U value due to addressing mode
			curFuncInvocation = OGRE_NEW FunctionInvocation(addressUFuncName, groupOrder, internalCounter++);
			curFuncInvocation->pushOperand(texcoord, Operand::OPS_IN, Operand::OPM_X);
			curFuncInvocation->pushOperand(psAtlasTextureCoord, Operand::OPS_OUT, Operand::OPM_X);
			psMain->addAtomInstance(curFuncInvocation);

			//calculate the V value due to addressing mode
			curFuncInvocation = OGRE_NEW FunctionInvocation(addressVFuncName, groupOrder, internalCounter++);
			curFuncInvocation->pushOperand(texcoord, Operand::OPS_IN, Operand::OPM_Y);
			curFuncInvocation->pushOperand(psAtlasTextureCoord, Operand::OPS_OUT, Operand::OPM_Y);
			psMain->addAtomInstance(curFuncInvocation);

			//sample the texel color
			curFuncInvocation = OGRE_NEW FunctionInvocation(SGX_FUNC_ATLAS_SAMPLE, groupOrder, internalCounter++);
			curFuncInvocation->pushOperand(sampler, Operand::OPS_IN);
			curFuncInvocation->pushOperand(texcoord, Operand::OPS_IN, Operand::OPM_X | Operand::OPM_Y);
			curFuncInvocation->pushOperand(psAtlasTextureCoord, Operand::OPS_IN);
			curFuncInvocation->pushOperand(mPSInpTextureDatas[j], Operand::OPS_IN);
			curFuncInvocation->pushOperand(texel, Operand::OPS_OUT);
			psMain->addAtomInstance(curFuncInvocation);
		}
	}
	return true;
}

//-----------------------------------------------------------------------
const char* TextureAtlasSampler::getAdressingFunctionName(TextureUnitState::TextureAddressingMode mode)
{
	switch (mode)
	{
	case TextureUnitState::TAM_WRAP: return SGX_FUNC_ATLAS_WRAP; 
	case TextureUnitState::TAM_MIRROR: return SGX_FUNC_ATLAS_MIRROR; 
	case TextureUnitState::TAM_CLAMP: return SGX_FUNC_ATLAS_CLAMP; 
	case TextureUnitState::TAM_BORDER: return SGX_FUNC_ATLAS_BORDER; 
	}
	return NULL;
}


//-----------------------------------------------------------------------
void TextureAtlasSampler::copyFrom(const SubRenderState& rhs)
{
	const TextureAtlasSampler& rhsColour = static_cast<const TextureAtlasSampler&>(rhs);

	mAtlasTexcoordPos = rhsColour.mAtlasTexcoordPos;
	mAtlasTextureStart = rhsColour.mAtlasTextureStart;
	mAtlasTextureCount = rhsColour.mAtlasTextureCount;
	mAtlasTableData = rhsColour.mAtlasTableData;
}

//-----------------------------------------------------------------------
void TextureAtlasSampler::updateGpuProgramsParams(Renderable* rend, Pass* pass,  const AutoParamDataSource* source, const LightList* pLightList)
{
	if (mIsTableDataUpdated == false)
	{
		mIsTableDataUpdated = true;
		GpuProgramParametersSharedPtr vsGpuParams = pass->getVertexProgramParameters();
		vector<float>::type buffer(mAtlasTableData->size() * 4);
		for(size_t i = 0 ; i < mAtlasTableData->size() ; ++i)
		{
			buffer[i*4] = (*mAtlasTableData)[i].posU;
			buffer[i*4 + 1] = (*mAtlasTableData)[i].posV;
			buffer[i*4 + 2] = (*mAtlasTableData)[i].width;
			buffer[i*4 + 3] = (*mAtlasTableData)[i].height;
		}
				

		vsGpuParams->setNamedConstant(mVSTextureTable->getName(), (const float*)(&(buffer[0])), mAtlasTableData->size()); 
	}
}

//-----------------------------------------------------------------------
bool TextureAtlasSampler::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass)
{
	mAtlasTexcoordPos = 0; 
	mAtlasTextureStart = 0;
	mAtlasTextureCount = 0;
	mAtlasTableData.setNull();

	const TextureAtlasSamplerFactory& factory = TextureAtlasSamplerFactory::getSingleton();

	unsigned short texCount = srcPass->getNumTextureUnitStates();
	for(unsigned short i = 0 ; i < texCount ; ++i)
	{
		TextureUnitState* pState = srcPass->getTextureUnitState(i);
		
		const TextureAtlasTablePtr& table = factory.getTextureAtlasTable(pState->getTextureName()); 
		if (table.isNull() == false)
		{
			//atlas texture only support 2d textures
			assert(pState->getTextureType() == TEX_TYPE_2D);
			if (pState->getTextureType() != TEX_TYPE_2D)
			{
				mAtlasTextureCount = 0;
				break;
			}

			if (mAtlasTableData.isNull())
			{
				mAtlasTableData = table;
				mAtlasTextureStart = i;
			}
			
			assert(mAtlasTableData == table);
			mTextureAddressings[mAtlasTextureCount] = pState->getTextureAddressingMode();
			++mAtlasTextureCount;

			if (mAtlasTextureCount == TAS_MAX_TEXTURES)
			{
				//At this time the shader does not support more than TAS_MAX_TEXTURES (4) atlas textures
				break;
			}
		}
		else if (mAtlasTableData.isNull() == false)
		{
			//only support consecutive textures in texture atlas
			break;
		}
	}
	
	//calculate the postiion of the indexes 
	mAtlasTexcoordPos = factory.getTableIndexPositionOffset();
	if (factory.getTableIndexPositionMode() == TextureAtlasSamplerFactory::ipmRelative)
	{
		mAtlasTexcoordPos += texCount - 1;
	}
	
	return mAtlasTextureCount != 0;
}

TextureAtlasSamplerFactory::TextureAtlasSamplerFactory() :
	mIndexPositionMode(ipmRelative),
	mIndexPositionOffset(1)
{

}

//-----------------------------------------------------------------------
const String& TextureAtlasSamplerFactory::getType() const
{
	return TextureAtlasSampler::Type;
}

//-----------------------------------------------------------------------
SubRenderState*	TextureAtlasSamplerFactory::createInstance(ScriptCompiler* compiler, 
													PropertyAbstractNode* prop, Pass* pass, SGScriptTranslator* translator)
{
	return NULL;
}

//-----------------------------------------------------------------------
void TextureAtlasSamplerFactory::writeInstance(MaterialSerializer* ser, SubRenderState* subRenderState, 
										Pass* srcPass, Pass* dstPass)
{
}

//-----------------------------------------------------------------------
bool TextureAtlasSamplerFactory::addTexutreAtlasDefinition( DataStreamPtr stream, TextureAtlasTable * textureAtlasTable )
{
	stream->seek(0);

	bool isSuccess = false;
	if (stream->isReadable() == true)
	{
		TextureAtlasMap tmpMap;
		
		while (stream->eof() == false)
		{
			String line = stream->getLine(true);
			size_t nonWhiteSpacePos = line.find_first_not_of(" \t\r\n");
			//check this is a line with information
			if ((nonWhiteSpacePos != String::npos) && (line[nonWhiteSpacePos] != '#'))
			{
				//parse the line
				vector<String>::type strings = StringUtil::split(line, ",\t");
				
				if (strings.size() > 8)
				{
					String textureName = strings[1];

					TextureAtlasMap::iterator it = tmpMap.find(textureName);
					if (tmpMap.find(textureName) == tmpMap.end())
					{
						it = tmpMap.insert(TextureAtlasMap::value_type(textureName, TextureAtlasTablePtr(new TextureAtlasTable))).first;
					}
					
					// file line format:  <original texture filename>/t/t<atlas filename>, <atlas idx>, <atlas type>, <woffset>, <hoffset>, <depth offset>, <width>, <height>
					//                              0                           1              2            3             4          5          6               7        8
					TextureAtlasRecord newRecord(
						strings[0], // original texture filename
						strings[1], // atlas filename
						(float)StringConverter::parseReal(strings[4]), // woffset
						(float)StringConverter::parseReal(strings[5]), // hoffset
						(float)StringConverter::parseReal(strings[7]), // width
						(float)StringConverter::parseReal(strings[8]), // height
						it->second->size() // texture index in atlas texture
						);

					it->second->push_back(newRecord);
					if (textureAtlasTable != NULL)
					{
						textureAtlasTable->push_back(newRecord);
					}

					isSuccess = true;
				}
			}
		}

		//place the information in the main texture
		TextureAtlasMap::const_iterator it = tmpMap.begin();
		TextureAtlasMap::const_iterator itEnd = tmpMap.end();
		for(;it != itEnd; ++it)
		{
			mAtlases[it->first] = it->second;
		}
	}
	return isSuccess;
}

//-----------------------------------------------------------------------
void TextureAtlasSamplerFactory::setTextureAtlasTable(const String& textureName, const TextureAtlasTablePtr& atlasData)
{
	if ((atlasData.isNull() == true) || (atlasData->empty()))
		removeTextureAtlasTable(textureName);
	else mAtlases.insert(TextureAtlasMap::value_type(textureName, atlasData));
}

//-----------------------------------------------------------------------
void TextureAtlasSamplerFactory::removeTextureAtlasTable(const String& textureName)
{
	mAtlases.erase(textureName);
}

//-----------------------------------------------------------------------
void TextureAtlasSamplerFactory::removeAllTextureAtlasTables()
{
	mAtlases.clear();
}

//-----------------------------------------------------------------------
const TextureAtlasTablePtr& TextureAtlasSamplerFactory::getTextureAtlasTable(const String& textureName) const
{
	TextureAtlasMap::const_iterator it = mAtlases.find(textureName);
	if (it != mAtlases.end())
	{
		return it->second;
	}
	else return c_BlankAtlasTable;
}


///Set the position of the atlas table indexes within the texcoords of the vertex data
void TextureAtlasSamplerFactory::setTableIndexPosition(IndexPositionMode mode, ushort offset)
{
	//It is illogical for offset to be 0. Go read the explanation in the h file.
	assert(offset > 0);
	mIndexPositionMode = mode;
	mIndexPositionOffset = offset;
}

///Get the positioning mode of the atlas table indexes within the texcoords of the vertex data
TextureAtlasSamplerFactory::IndexPositionMode TextureAtlasSamplerFactory::getTableIndexPositionMode() const
{
	return mIndexPositionMode;
}

///Get the offset of the atlas table indexes within the texcoords of the vertex data
ushort TextureAtlasSamplerFactory::getTableIndexPositionOffset() const
{
	return mIndexPositionOffset;
}

//-----------------------------------------------------------------------
SubRenderState*	TextureAtlasSamplerFactory::createInstanceImpl()
{
	return OGRE_NEW TextureAtlasSampler;
}


}
}

#endif