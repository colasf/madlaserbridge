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

#pragma once
#include "DatagramSocket/DatagramSocket.h"
#include "UdpGeomDefs.h"

#include "SOP_CPlusPlusBase.h"
#include <string>

#include <vector>
#include <map>
#include <string>



// To get more help about these functions, look at SOP_CPlusPlusBase.h
class 
	MadLaserBridge : public SOP_CPlusPlusBase
{
public:

	MadLaserBridge(const OP_NodeInfo* info);

	virtual ~MadLaserBridge();

	virtual void	getGeneralInfo(SOP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;

	virtual void	execute(SOP_Output*, const OP_Inputs*, void* reserved) override;


	virtual void executeVBO(SOP_VBOOutput* output, const OP_Inputs* inputs,
							void* reserved) override;


	virtual int32_t getNumInfoCHOPChans(void* reserved) override;

	virtual void getInfoCHOPChan(int index, OP_InfoCHOPChan* chan, void* reserved) override;

	virtual bool getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved) override;

	virtual void getInfoDATEntries(int32_t index, int32_t nEntries,
									OP_InfoDATEntries* entries,
									void* reserved) override;

	virtual void setupParameters(OP_ParameterManager* manager, void* reserved) override;
	virtual void pulsePressed(const char* name, void* reserved) override;

private:
	void push16bits(std::vector<unsigned char>& fullData, unsigned short value);
	void push32bits(std::vector<unsigned char>& fullData, int value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value);
	void pushPoint(std::vector<unsigned char>& fullData, Position& pointPosition, Color& pointColor);
	bool validatePrimitiveDat(const OP_DATInput* primitive, int numPrimitive);
	std::map<std::string, float> getMetadata(const OP_DATInput* primitive, int primitiveIndex);

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo*		myNodeInfo;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the SOP
	int32_t					myExecuteCount;


	double					myOffset;
	std::string             myChopChanName;
	float                   myChopChanVal;
	std::string             myChop;

	std::string             myDat;

	int						myNumVBOTexLayers;

	DatagramSocket* socket;

	double animTime = 0;
	unsigned char frameNumber = 0;
};
