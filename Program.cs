using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace WebAPI
{
    public class Program
    {
        private const int DCCSignalLeftPin = 20;
        private const int DCCSignalRightPin = 21;

        public unsafe static void Main(string[] args)
        {
            WiringPi.Init.WiringPiSetupSys();

            WiringPi.GPIO.pinMode(16, (int) WiringPi.GPIO.GPIOpinmode.Output);
            WiringPi.GPIO.pinMode(17, (int) WiringPi.GPIO.GPIOpinmode.Output);

            AccessoryPacketDecoder.Init.PacketReady ready = NewPacket;

            AccessoryPacketDecoder.Init.Start(DCCSignalLeftPin,
                DCCSignalRightPin,
                NewPacket);

            BuildWebHost(args).Run();
        }

        public unsafe static void NewPacket(byte* packet, int packetLength)
        {
            string packetStr = string.Empty;

            for(int i =0; i < packetLength; i++)
            {
                packetStr += " 0x" + packet[i].ToString("X2");
            }

            Console.WriteLine(packetStr);
        }

        public static IWebHost BuildWebHost(string[] args) =>
            WebHost.CreateDefaultBuilder(args)
                .UseUrls("http://*:5000")
                .UseStartup<Startup>()
                .Build();
    }
}
