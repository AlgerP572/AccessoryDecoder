using System;
using System.Runtime.InteropServices;

namespace AccessoryPacketDecoder
{

    public static class Init
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public unsafe delegate void PacketReady(byte* packet, int packetLength);

        [DllImport("AccesoryPacketDecoder.so", EntryPoint = "Start", CallingConvention = CallingConvention.Cdecl)]     //This is an example of how to call a method / function in a c library from c#
        public static extern void Start(int dccPinLeft, int dccPinRight, PacketReady newPacket);
    }

    
}
