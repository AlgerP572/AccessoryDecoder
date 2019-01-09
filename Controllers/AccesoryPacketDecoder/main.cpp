#include <cstdio>
#include <stdlib.h>
#include <string.h>

#include <wiringPi.h>

#define	DEBUGLED	16

// local functions
void DccPinLeftIsr();
unsigned char GetDccByte(volatile unsigned char* bitBuffer, const int firstBitIndex);

// local types
typedef  void(*PacketReady)(unsigned char* packet, int packetSize);

// By convention using a Null termination to indicate end of packet.
// This allows use of strxxx for easier manipulation when needed.
#define ENDOFPACKET NULL
#define PacketBufferSize 255
#define DCCBitBufferSize 2048

#define DccOneBit 1 // value to save when dcc one bit is detected.
#define DccZeroBit 0 // value to save when dcc zero bit is detected.

// Use this with caution setting a non-zero value will cause the
// firmware and capture some bits which may be out-of-specification.
//
// But this can help in some situations where the timing issue is
// caused by linux latencies...  Also note: if you go too big the
// firmware will no longer be able to discriminate between dcc bit
// zero and dcc bit 1 properly...
#define LinuxLatency 10  // 탎ec (lower is better)

#define MinimumOneBitDuration (52 * 2) - LinuxLatency // 탎ec (only counting full bits here)
#define MaximumOneBitDuration (64 * 2) + LinuxLatency // 탎ec (only counting full bits here)

#define MinimumZeroBitDuration 90 * 2 // 탎ec (only counting full bits here)
#define MaximumZeroBitDuration 10000 * 2 // 탎ec (only counting full bits here)

#define MinimumOneBitsForValidPreamble 10 // Used to determine the start of a new packet.

#define BitsInADCCByte 9 // This is the 8 bits of the byte plus the next bit for packet end or next byte

volatile unsigned char dccPacket[PacketBufferSize] = "Last received DCC packet is in this buffer.\n\0";
volatile unsigned char dccBits[DCCBitBufferSize] = "Last received DCC bits is in this buffer.\n\0";
volatile int dccBitDurations[DCCBitBufferSize];

volatile int startBitTime = 0;
volatile int endBitTime = 0;
volatile int bitDuration = 0;

volatile int totalBitCount = 0;
volatile int consecutiveOneBitCount = 0;
volatile int consecutiveZeroBitCount = 0;

volatile int numPreambleBits = 0;
volatile bool decodingPacket = false;

volatile int numDataByteBits = 0;
volatile int numDccPacketBytes = 0;

volatile PacketReady packetReceived;

volatile int DccPinLeft;
volatile int DccPinRight;

extern "C" void Start(int dccPinLeft, int dccPinRight, PacketReady packetReady)
{
	// This library assumes that WiringPi setup and init is
	// handled by the main executable program...
	//
	// This section will set the modes for pins that it
	// relies on and connect the interrupt handlers to
	// the pins.
	printf("Hello from AccesoryPacketDecoder!\n");

	DccPinLeft = dccPinLeft;
	DccPinRight = dccPinRight;

	pinMode(DccPinLeft, INPUT);
	printf("Pin %d is dccPinLeft\n", dccPinLeft);
	pinMode(DccPinRight, INPUT);
	printf("Pin %d is dccPinRight\n", dccPinRight);

	if (packetReady == NULL)
	{
		printf("Packet Ready not specified packets will not be forwarded\n");
		return;
	}

	printf("Packet Ready  function @ 0x%x, Testing with user message next\n", packetReady);

	
	// Verify the callback will work by sending a test.
	packetReceived = packetReady;
	packetReceived((unsigned char*) dccPacket, 44);
	

	// Ok init the packet buffer to receive packets.
	// This allows for easier debugging...
	memset((unsigned char*) dccPacket,
		0,
		PacketBufferSize);

	wiringPiISR(dccPinLeft,
		INT_EDGE_RISING,
		DccPinLeftIsr);
}

void DccPinLeftIsr()
{
	endBitTime = micros();

	// Width the bit determines if its a 0 or 1 see NMRA standard for details.
	//
	// Once every ~71 minutes this will be negative due to clock roll over.
	// The result is the bit is skipped, yielding an invalid packet. We should
	// just pick up the next one. This seems ok in order to avoid an abs() call
	// here.  Packets are proably missed more often then this anyway due to
	// electrical noise, linux timing errors, etc...
	//
	// Its in the design and the NMRA standard for this code to be able to carry
	// on if it misses a bit or packet.
	bitDuration = endBitTime - startBitTime;

	// This time is also the start of the next bit...
	startBitTime = endBitTime;

	if ((bitDuration >= MinimumOneBitDuration) &&
		(bitDuration <= MaximumOneBitDuration))
	{
//		digitalWrite(DEBUGLED, HIGH);

		// here just using bytes to represent bits since memory
		// layout for bitfields is dependent on the compiler.
		//
		// The presumption is that byte aligned data will be
		// faster and more portable.
		dccBits[totalBitCount] = (unsigned char)DccOneBit;
		dccBitDurations[totalBitCount] = bitDuration;
		consecutiveOneBitCount++;
		consecutiveZeroBitCount = 0;
		totalBitCount++;

		if (decodingPacket)
		{
			numDataByteBits++;

			if (numDataByteBits == BitsInADCCByte)
			{
				int i = 0;
				if (i != 0)
				{
					i++;
					wiringPiDetachISR(DccPinLeft);
				}

				// This is end-of-packet...
				digitalWrite(DEBUGLED, HIGH);
				unsigned char dccByte = GetDccByte(dccBits, totalBitCount - BitsInADCCByte);

				numDataByteBits = 0;
				decodingPacket = false;

				dccPacket[numDccPacketBytes] = dccByte;
				numDccPacketBytes++;
				
				// For now just starting again. Is it worth capturing multiple packets?
				// i.e. treating the buffer like a rolling fifo?
				totalBitCount = 0;
				bitDuration = 0;
				consecutiveOneBitCount = 0;
				consecutiveZeroBitCount = 0;
				digitalWrite(DEBUGLED, HIGH);

				if (packetReceived != NULL)
				{
					packetReceived((unsigned char*) dccPacket, numDccPacketBytes);
				}
				numDccPacketBytes = 0;
			}
		}

//		digitalWrite(DEBUGLED, LOW);
	}
	else if ((bitDuration >= MinimumZeroBitDuration) &&
		(bitDuration <= MaximumZeroBitDuration))
	{
//		digitalWrite(DEBUGLED, HIGH);

		dccBits[totalBitCount] = (unsigned char)DccZeroBit;
		dccBitDurations[totalBitCount] = bitDuration;
		consecutiveZeroBitCount++;
		totalBitCount++;
		if (decodingPacket)
		{
			numDataByteBits++;

			if (numDataByteBits == BitsInADCCByte)
			{
				// This is end-of-byte
				digitalWrite(DEBUGLED, HIGH);
				unsigned char dccByte = GetDccByte(dccBits, totalBitCount - BitsInADCCByte);
				numDataByteBits = 0;

				dccPacket[numDccPacketBytes] = dccByte;
				numDccPacketBytes++;
				digitalWrite(DEBUGLED, LOW);
			}
		}

		if (consecutiveOneBitCount >= MinimumOneBitsForValidPreamble)
		{
			digitalWrite(DEBUGLED, HIGH);
			numPreambleBits = consecutiveOneBitCount;
			decodingPacket = true;
					
			// Don't need to care about more than 10 preamble bits but its easier to capture
			// two bytes here and firmware can disregard bits that are not relevant...
			//
			// Under normal conditions this code will capture all 14 preamble bits
			// and the pattern preambleByte1 = 0x3F preambleByte2 = 0xFF will indicate
			// start packet capture... but other valid preambleByte1 values are 0x03, 0x07,
			// 0x0F, 0x1F etc... 0x01 is not valid because this is ambiguous with packet end.
			unsigned char preambleByte1 = GetDccByte(dccBits, totalBitCount - (BitsInADCCByte + 8));
			unsigned char preambleByte2 = GetDccByte(dccBits, totalBitCount - BitsInADCCByte);

			dccPacket[0] = preambleByte1;
			dccPacket[1] = preambleByte2;
			numDccPacketBytes = 2;

			digitalWrite(DEBUGLED, LOW);
		}
		consecutiveOneBitCount = 0;

//		digitalWrite(DEBUGLED, LOW);
	}
	else
	{
//		printf("Timing Error. Bit Duration %d \n", bitDuration);

		// timing is not within DCC spec start again...		
		// totalBitCount = 0;
		bitDuration = 0;
		consecutiveOneBitCount = 0;
		consecutiveZeroBitCount = 0;
		decodingPacket = false;

		digitalWrite(DEBUGLED, HIGH);
		digitalWrite(DEBUGLED, LOW);
		digitalWrite(DEBUGLED, HIGH);
	}

	if (totalBitCount >= DCCBitBufferSize)
	{
		// ran off the buffer. start again...		
		totalBitCount = 0;
		bitDuration = 0;
		consecutiveOneBitCount = 0;
		consecutiveZeroBitCount = 0;
		decodingPacket = false;
	}
}

unsigned char GetDccByte(volatile unsigned char* bitBuffer, const int msbBitIndex)
{
	int dccByte = 0;
	const int bitNumber = 7;  // 0 based bit number for this byte.

	// This code assumes msb first and adds one until it reaches the lsb.
	for (int i = 0; i <= bitNumber; i++)
	{
		dccByte |= bitBuffer[msbBitIndex + i] << (bitNumber - i);
	}
	return (unsigned char) dccByte;
}