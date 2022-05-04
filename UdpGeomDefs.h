#pragma once

/*
 *  Udp Geom is a minimal protocol to transfer 2D colored pathes from a source to
 *  one or multiple receivers. It has mostly been developped to transfer laser path
 *  from a software to another using multicast UDP.
 *
 *  Requirements are:
 *      Transfer over network of 2D geometry path with color along the path
 *      Make it simple to implement on both sides
 *      Make it work on almost any network - no specific hardware requirement or os settings tricks
 *      Make it extendable while not increasing bandwidth requirements in most common cases
 *      Avoid using unecessary bandwidth:
 *          16 bits XY is enough for any laser control, so don't use 32 bits floating point
 *          16 bits color is a choice to simplify implementation, but 12 bits per component would be enough
 *
 *  Implementation in Sender
 *      A software can send multiple different path streams. The sender will let the user
 *      setup a 32 bits number to identify the stream (could be a random generated value when instanciating
 *      the sending component) and a sender name string (UTF8) that can be used to display a
 *      readable source name is the receiving application.
 *      The sender identifier should not change when reloading a project file so the receiver can
 *      reconnect the stream.
 *
 *  Implementation in Receiver
 *      The receiver should handle the fact that multiple sender can be sending packets.
 *      It should be as reactive as possible to reduce latency and improve synchronization.
 *      The receiver should accept any packet size (chunk size could be adjusted on sender side)
 *
 *  This current version is a prototype. Possible improvements / extensions:
 *      Synchronization
 *          - TCP/IP transport might improve smoothness. Also if the receiver can supply information
 *            that it has used the last received frame, the sender could adjust to the receiver framerate...
 *      New Data Formats:
 *          - XY_U16_SingleRGB: if you send a path of 2000 points with the same color,
 *                              you can reduce by 60% the packet size by removing color
 *          - XYRGBU1U2U3_U16: yellow diode ? more diodes, some applications might handle that
 *          - XYZRGB_U16: providing the Z would let the receiver handle the projection from 3D to 2D.
 *      Laser Rasterization Settings:
 *          If you use a software for path generation (sender) and a receiver for the display,
 *          you might want to provide, for each path, some parameters that the receiver could
 *          use for rasterizing the 2D geometry pathes to an ILDA frame. For instance:
 *              - Should we skip scanning long black sections ? (parameter "Skip Black Sections" ?)
 *              - Should we prior scan speed or path render precision ? (parameter "Max Scan Speed" in radians/ms ?)
 *              - Should we optimize angles for this path ? In some case you might want (ie text display) but is some not (ie generative noisy curved shapes)
 *
 *
 *
*/

// Header String
#define GEOM_UDP_HEADER_STRING "Geom-UDP"
// Protocol Version
#define GEOM_UDP_PROTOCOL_VERSION 0
// Data Formats
#define GEOM_UDP_DATA_FORMAT_XYRGB_U16 0
// Maximum chunk size
#define GEOM_UDP_MAX_DATA_BYTES_PER_PACKET 1400
// Geom UDP IP v4 address = 239.255.5.1
#define GEOM_UDP_IP ((239<<24) + (255<<16) + (5<<8) + 1)
// Geom UDP port = 5583
#define GEOM_UDP_PORT 5583

struct GeomUdpHeader {
    char headerString[8];           // = "Geom-UDP"
    unsigned char protocolVersion;  // 0 at the moment
    //unsigned int senderIdentifier;  // 4 bytes - used to identify the sourc
    char senderName[32];            // 32 bytes UTF8 null terminated string
    unsigned char frameNumber;
    unsigned char chunkCount;
    unsigned char chunkNumber;
    // Data
};
