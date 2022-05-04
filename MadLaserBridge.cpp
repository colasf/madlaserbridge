/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/


#include "MadLaserBridge.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <iostream>
#include <vector>




#ifndef M_PI // M_PI not defined on Windows
	#define M_PI 3.14159265358979323846
#endif

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

	DLLEXPORT
	void
	FillSOPPluginInfo(SOP_PluginInfo *info)
	{
		// Always return SOP_CPLUSPLUS_API_VERSION in this function.
		info->apiVersion = SOPCPlusPlusAPIVersion;

		// The opType is the unique name for this TOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("MadLaserBridge");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Mad Laser Bridge");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("MLB");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Tyrell");
		info->customOPInfo.authorEmail->setString("colas.fiszman@gmail.com");

		// This SOP works with 0 or 1 inputs
		info->customOPInfo.minInputs = 0;
		info->customOPInfo.maxInputs = 0;

	}

	DLLEXPORT
	SOP_CPlusPlusBase*
	CreateSOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per SOP that is using the .dll
		return new MadLaserBridge(info);
	}

	DLLEXPORT
	void
	DestroySOPInstance(SOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the SOP using that instance is deleted, or
		// if the SOP loads a different DLL
		delete (MadLaserBridge*)instance;
	}

};


MadLaserBridge::MadLaserBridge(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myOffset = 0.0;
	myChop = "";

	myChopChanName = "";
	myChopChanVal = 0;

	myDat = "N/A";

	socket = new DatagramSocket (INADDR_ANY, 0);;
}

MadLaserBridge::~MadLaserBridge()
{
	delete socket;
}

void
MadLaserBridge::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = false;

	ginfo->cookEveryFrame = true;

	//if direct to GPU loading:
	bool directGPU = inputs->getParInt("Gpudirect") != 0 ? true : false;
	ginfo->directToGPU = directGPU;

}

void
MadLaserBridge::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	myExecuteCount++;

	if (inputs->getNumInputs() > 0)
	{
		inputs->enablePar("Reset", 0);	// not used
		inputs->enablePar("Shape", 0);	// not used
		inputs->enablePar("Scale", 0);  // not used

		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		const Position* ptArr = sinput->getPointPositions();
		//const Vector* normals = nullptr;
		const Color* colors = nullptr;
		//const TexCoord* textures = nullptr;
		int32_t numTextures = 0;


		if (sinput->hasColors())
		{
			colors = sinput->getColors()->colors;
		}

		std::vector<unsigned char> fullData;
		fullData.reserve(65536);

		// Write Format Data
		fullData.push_back(GEOM_UDP_DATA_FORMAT_XYRGB_U16);


		for (int i = 0; i < sinput->getNumPrimitives(); i++)
		{

			std::cout << "-------------------- primitive : " << i << std::endl;

			const SOP_PrimitiveInfo primInfo = sinput->getPrimitive(i);

			const int32_t* primVert = primInfo.pointIndices;
			std::cout << primInfo.numVertices << std::endl;

			// Write point count - LSB first
			fullData.push_back((primInfo.numVertices >> 0) & 0xFF);
			fullData.push_back((primInfo.numVertices >> 8) & 0xFF);

			for (int j = 0; j < primInfo.numVertices; j++) {

				std::cout << j << std::endl;
				Position pointPosition = ptArr[primVert[j]];
				std::cout << "x : " << pointPosition.x << " y : " << pointPosition.y << " z : " << pointPosition.z << std::endl;

				if (sinput->hasColors()) {
					Color pointColor = colors[primVert[j]];
					std::cout << "r : " << pointColor.r << " g : " << pointColor.g << " b : " << pointColor.b << " a : " << pointColor.a << std::endl;
				} 


				const auto x16Bits = static_cast<unsigned short>(((pointPosition.x + 1) / 2) * 65535);
				const auto y16Bits = static_cast<unsigned short>(((pointPosition.y + 1) / 2) * 65535);
				// Push X - LSB first
				fullData.push_back(static_cast<unsigned char>((x16Bits >> 0) & 0xFF));
				fullData.push_back(static_cast<unsigned char>((x16Bits >> 8) & 0xFF));
				// Push Y - LSB first
				fullData.push_back(static_cast<unsigned char>((y16Bits >> 0) & 0xFF));
				fullData.push_back(static_cast<unsigned char>((y16Bits >> 8) & 0xFF));
				// Push R - LSB first
				fullData.push_back(0xFF);
				fullData.push_back(0xFF);
				// Push G - LSB first
				fullData.push_back(0xFF);
				fullData.push_back(0xFF);
				// Push B - LSB first
				fullData.push_back(0xFF);
				fullData.push_back(0xFF);
			}

		}


		size_t chunksCount64 = 1 + fullData.size() / GEOM_UDP_MAX_DATA_BYTES_PER_PACKET;
		if (chunksCount64 > 255) {
			throw std::runtime_error("Protocol doesn't accept sending "
				"a packet that would be splitted "
				"in more than 255 chunks");
		}

		size_t written = 0;
		unsigned char chunkNumber = 0;
		unsigned char chunksCount = static_cast<unsigned char>(chunksCount64);
		while (written < fullData.size()) {
			// Write packet header - 8 bytes
			GeomUdpHeader header;
			strncpy(header.headerString, GEOM_UDP_HEADER_STRING, sizeof(header.headerString));
			header.protocolVersion = 0;
			strncpy(header.senderName, "Sample Sender", sizeof(header.senderName));
			header.frameNumber = frameNumber;
			header.chunkCount = chunksCount;
			header.chunkNumber = chunkNumber;

			// Prepare buffer
			std::vector<unsigned char> packet;
			size_t dataBytesForThisChunk = std::min<size_t>(fullData.size() - written, GEOM_UDP_MAX_DATA_BYTES_PER_PACKET);
			packet.resize(sizeof(GeomUdpHeader) + dataBytesForThisChunk);
			// Write header
			memcpy(&packet[0], &header, sizeof(GeomUdpHeader));
			// Write data
			memcpy(&packet[sizeof(GeomUdpHeader)], &fullData[written], dataBytesForThisChunk);
			written += dataBytesForThisChunk;

			// Now send chunk packet
			GenericAddr destAddr;
			destAddr.family = AF_INET;
			// Multicast 
			//destAddr.ip = GEOM_UDP_IP;
			// Unicast on localhost
			destAddr.ip = ((192 << 24) + (168 << 16) + (1 << 8) + 4);
			destAddr.port = GEOM_UDP_PORT;
			socket->sendTo(destAddr, &packet.front(), static_cast<unsigned int>(packet.size()));

			chunkNumber++;
		}

		std::cout << "Sent frame " << std::to_string(frameNumber) << std::endl;

		animTime += 1 / 60.;
		frameNumber++;

	}

}

void
MadLaserBridge::executeVBO(SOP_VBOOutput* output,
						const OP_Inputs* inputs,
						void* reserved)
{

}

//-----------------------------------------------------------------------------------------------------
//								CHOP, DAT, and custom parameters
//-----------------------------------------------------------------------------------------------------

int32_t
MadLaserBridge::getNumInfoCHOPChans(void* reserved)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send 4 channels.
	return 4;
}

void
MadLaserBridge::getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan* chan, void* reserved)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name->setString("offset");
		chan->value = (float)myOffset;
	}

	if (index == 2)
	{
		chan->name->setString(myChop.c_str());
		chan->value = (float)myOffset;
	}

	if (index == 3)
	{
		chan->name->setString(myChopChanName.c_str());
		chan->value = myChopChanVal;
	}
}

bool
MadLaserBridge::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 3;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
MadLaserBridge::getInfoDATEntries(int32_t index,
								int32_t nEntries,
								OP_InfoDATEntries* entries,
								void* reserved)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "executeCount");
#else // macOS
		strlcpy(tempBuffer, "executeCount", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", myExecuteCount);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "offset");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%g", myOffset);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 2)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "DAT input name");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		strcpy_s(tempBuffer, myDat.c_str());
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}
}



void
MadLaserBridge::setupParameters(OP_ParameterManager* manager, void* reserved)
{
	// CHOP
	{
		OP_StringParameter	np;

		np.name = "Chop";
		np.label = "CHOP";

		OP_ParAppendResult res = manager->appendCHOP(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// scale
	{
		OP_NumericParameter	np;

		np.name = "Scale";
		np.label = "Scale";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] = 10.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// shape
	{
		OP_StringParameter	sp;

		sp.name = "Shape";
		sp.label = "Shape";

		sp.defaultValue = "Cube";

		const char *names[] = { "Cube", "Triangle", "Line" };
		const char *labels[] = { "Cube", "Triangle", "Line" };

		OP_ParAppendResult res = manager->appendMenu(sp, 3, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}

	// GPU Direct
	{
		OP_NumericParameter np;

		np.name = "Gpudirect";
		np.label = "GPU Direct";

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
MadLaserBridge::pulsePressed(const char* name, void* reserved)
{
	if (!strcmp(name, "Reset"))
	{
		myOffset = 0.0;
	}
}

