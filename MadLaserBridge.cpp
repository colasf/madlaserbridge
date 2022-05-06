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
		info->customOPInfo.authorEmail->setString("colas@tyrell.studio");

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
	ginfo->directToGPU = false;

}

void MadLaserBridge::push16bits(std::vector<unsigned char>& fullData, unsigned short value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
}

void MadLaserBridge::push32bits(std::vector<unsigned char>& fullData, int value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
}

void MadLaserBridge::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, value);
}
void MadLaserBridge::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, *(int*)&value);
}

void MadLaserBridge::pushPoint(std::vector<unsigned char>& fullData, Position& pointPosition, Color& pointColor) {
	const auto x16Bits = static_cast<unsigned short>(((pointPosition.x + 1) / 2) * 65535);
	const auto y16Bits = static_cast<unsigned short>(((pointPosition.y + 1) / 2) * 65535);

	const auto r16Bits = static_cast<unsigned short>(((pointColor.r + 1) / 2) * 65535);
	const auto g16Bits = static_cast<unsigned short>(((pointColor.g + 1) / 2) * 65535);
	const auto b16Bits = static_cast<unsigned short>(((pointColor.b + 1) / 2) * 65535);

	// Push X - LSB first
	push16bits(fullData, x16Bits);
	// Push Y - LSB first
	push16bits(fullData, y16Bits);
	// Push R - LSB first
	push16bits(fullData, r16Bits);
	// Push G - LSB first
	push16bits(fullData, g16Bits);
	// Push B - LSB first
	push16bits(fullData, b16Bits);

}

bool MadLaserBridge::validatePrimitiveDat(const OP_DATInput* primitive, int numPrimitives) {
	// Check that the dat is table
	if (!primitive->isTable) {
		return false;
	}

	// Check that that the number of row match the number of primitive + title
	if (primitive->numRows != (numPrimitives + 1)) {
		return false;
	}

	// Check that the title of the third column is close
	if (strcmp(primitive->getCell(0, 2), "close") != 0)
	{
		return false;
	}

	return true;
}


std::map<std::string, float> MadLaserBridge::getMetadata(const OP_DATInput* primitive, int primitiveIndex) {
	std::map<std::string, float> metadata;
	
	// check how many metadata attribute the primitive dat contains
	int numMetadata = primitive->numCols - 4;

	// get the metadata from the dat
	for (int i = 0; i < numMetadata; i++) {
		std::string metadataName = primitive->getCell(0, 3 + i);
		float metadataValue = (float)std::strtod(primitive->getCell(primitiveIndex + 1, 3 + i), NULL);
		metadata[metadataName] = metadataValue;
	}

	return metadata;
}



void
MadLaserBridge::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	myExecuteCount++;

	// Get the primitive dat from the attribute
	const OP_DATInput* primitive = inputs->getParDAT("Primitive"); 

	// Only run if a SOP is connected on the first input and a
	// is set on the primitive parameter
	if (inputs->getNumInputs() > 0 && primitive)
	{
		// Get the input sop
		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		// Create the vector  will store our the data that need to be send to madLaser
		std::vector<unsigned char> fullData;
		fullData.reserve(65536);

		// Write Format Data
		fullData.push_back(GEOM_UDP_DATA_FORMAT_XYRGB_U16);

		// Check that the primitive dat is valid
		if(validatePrimitiveDat(primitive, sinput->getNumPrimitives()))
		{
			const Position* ptArr = sinput->getPointPositions();
			const Color* colors = nullptr;


			if (sinput->hasColors())
			{
				colors = sinput->getColors()->colors;
			}


			for (int i = 0; i < sinput->getNumPrimitives(); i++)
			{

				//std::cout << "-------------------- primitive : " << i << std::endl;

				// get the metadata
				std::map<std::string, float> metadata = getMetadata(primitive, i);

				// Write meta data count
				fullData.push_back(metadata.size());

				for (const auto& kv : metadata) {
					char charMetadata[9];
					std::copy(kv.first.begin(), kv.first.end(), charMetadata);

					pushMetaData(fullData, charMetadata, kv.second);
				}


				const SOP_PrimitiveInfo primInfo = sinput->getPrimitive(i);

				const int32_t* primVert = primInfo.pointIndices;
				//std::cout << primInfo.numVertices << std::endl;

				int numPoints = primInfo.numVertices;

				// check if the primitve is closed
				bool isClosed = false;
				if (strcmp(primitive->getCell(i+1, 2), "1") == 0) {
					//std::cout << "primitive is close" << std::endl;

					isClosed = true;

					// add one point to the count
					numPoints += 1;
				}

				//std::cout << numPoints << std::endl;

				// Write point count
				push16bits(fullData, numPoints);

				for (int j = 0; j < primInfo.numVertices; j++) {

					//std::cout << j << std::endl;
					Position pointPosition = ptArr[primVert[j]];
					//std::cout << "x : " << pointPosition.x << " y : " << pointPosition.y << " z : " << pointPosition.z << std::endl;

					
					Color pointColor(1.0f, 1.0f, 1.0f, 1.0f);

					if (sinput->hasColors()) {
						pointColor = colors[primVert[j]];

						//std::cout << "r : " << pointColor.r << " g : " << pointColor.g << " b : " << pointColor.b << " a : " << pointColor.a << std::endl;
					}

					pushPoint(fullData, pointPosition, pointColor);

				}

				// If the primitive is close add the first point at the end
				if (isClosed)
				{
					Position pointPosition = ptArr[primVert[0]];

					Color pointColor(1.0f, 1.0f, 1.0f, 1.0f);

					if (sinput->hasColors()) {
						pointColor = colors[primVert[0]];
					}

					pushPoint(fullData, pointPosition, pointColor);

				}
			}

		}
		else
		{
			//std::cout << "Invalid Primitive Dat" << std::endl;
		}

		// Check if we don't reach the maximum number of chunck
		size_t chunksCount64 = 1 + fullData.size() / GEOM_UDP_MAX_DATA_BYTES_PER_PACKET;
		if (chunksCount64 > 255) {
			throw std::runtime_error("Protocol doesn't accept sending "
				"a packet that would be splitted "
				"in more than 255 chunks");
		}

		// Get the ip address from the attribute
		int ip[4];
		inputs->getParInt4("Netaddress", ip[0], ip[1], ip[2], ip[3]);

		//std::cout << "Ip " 
			//<< std::to_string(ip[0]) << "." 
			//<< std::to_string(ip[1]) << "."
			//<< std::to_string(ip[2]) << "."
			//<< std::to_string(ip[3]) << std::endl;

		// Get the Unique identifier from the attribute
		int uid = inputs->getParInt("Uid");

		//std::cout << "Uid " << uid << std::endl;

		size_t written = 0;
		unsigned char chunkNumber = 0;
		unsigned char chunksCount = static_cast<unsigned char>(chunksCount64);
		while (written < fullData.size()) {
			// Write packet header - 8 bytes
			GeomUdpHeader header;
			strncpy(header.headerString, GEOM_UDP_HEADER_STRING, sizeof(header.headerString));
			header.protocolVersion = 0;
			header.senderIdentifier = uid; // Unique ID (so when changing name in sender, the receiver can just rename existing stream)
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
			destAddr.ip = ((ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3]);
			destAddr.port = GEOM_UDP_PORT;
			socket->sendTo(destAddr, &packet.front(), static_cast<unsigned int>(packet.size()));

			chunkNumber++;
		}

		//std::cout << "Sent frame " << std::to_string(frameNumber) << std::endl;

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
	//// CHOP
	//{
	//	OP_StringParameter	np;

	//	np.name = "Chop";
	//	np.label = "CHOP";

	//	OP_ParAppendResult res = manager->appendCHOP(np);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	//// scale
	//{
	//	OP_NumericParameter	np;

	//	np.name = "Scale";
	//	np.label = "Scale";
	//	np.defaultValues[0] = 1.0;
	//	np.minSliders[0] = -10.0;
	//	np.maxSliders[0] = 10.0;

	//	OP_ParAppendResult res = manager->appendFloat(np);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	//// shape
	//{
	//	OP_StringParameter	sp;

	//	sp.name = "Shape";
	//	sp.label = "Shape";

	//	sp.defaultValue = "Cube";

	//	const char *names[] = { "Cube", "Triangle", "Line" };
	//	const char *labels[] = { "Cube", "Triangle", "Line" };

	//	OP_ParAppendResult res = manager->appendMenu(sp, 3, names, labels);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	//// GPU Direct
	//{
	//	OP_NumericParameter np;

	//	np.name = "Gpudirect";
	//	np.label = "GPU Direct";

	//	OP_ParAppendResult res = manager->appendToggle(np);
	//	assert(res == OP_ParAppendResult::Success);
	//}

	//// pulse
	//{
	//	OP_NumericParameter	np;

	//	np.name = "Reset";
	//	np.label = "Reset";

	//	OP_ParAppendResult res = manager->appendPulse(np);
	//	assert(res == OP_ParAppendResult::Success);
	//}
	// 
	// Ip
	{
		OP_NumericParameter	np;

		np.name = "Netaddress";
		np.label = "Network Address";

		// Minimum values
		np.minValues[0] = 0;
		np.minValues[1] = 0;
		np.minValues[2] = 0;
		np.minValues[3] = 0;

		// Maximum values
		np.maxValues[0] = 255;
		np.maxValues[1] = 255;
		np.maxValues[2] = 255;
		np.maxValues[3] = 255;

		OP_ParAppendResult res = manager->appendInt(np, 4);

	}

	// Unique ID
	{
		OP_NumericParameter	np;

		np.name = "Uid";
		np.label = "Unique ID";

		OP_ParAppendResult res = manager->appendInt(np, 1);
	}

	// Primitive data
	{
		OP_StringParameter sopp;

		sopp.name = "Primitive";
		sopp.label = "Primitive";

		OP_ParAppendResult res = manager->appendDAT(sopp);
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

