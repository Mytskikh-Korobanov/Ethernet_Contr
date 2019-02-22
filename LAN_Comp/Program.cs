using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.Net.Sockets;
using System.IO;

namespace LAN_Comp
{
    class Program
    {
        static void Main(string[] args)
        {
            string IP_Address = "169.254.12.249";
            int Port = 49152;
            Socket Socket;
            IPEndPoint Ip_Point = new IPEndPoint(IPAddress.Parse(IP_Address), Port);
            Socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            Socket.Connect(Ip_Point);
            byte[] asc = new byte[9];
            asc[0] = 0XAA;
            asc[1] = 0X01;
            asc[2] = 0X6C;
            asc[3] = 0X00;
            asc[4] = 0X00;
            asc[5] = 0X00;
            asc[6] = 0X00;
            asc[7] = 0X38;
            asc[8] = 0XFF;
            Socket.Send(asc);
            System.Threading.Thread.Sleep(2000);
            byte[] Ans = new byte[9];
            Socket.Receive(Ans, Ans.Length, 0);
            Console.WriteLine("Version = " + Ans[3] + "." + Ans[4]);
            Console.ReadLine();
            Socket.Close();
        }
    }
}
