#include <Ethernet.h>
#include <SPI.h>
#include <dht.h>
#include <EEPROM.h>

byte Version[] = {0, 1}; // версия прошивки

byte Num_PCB = 1; // номер платы
int port = 49152;
byte mac[] = {0x70, 0x4D, 0x7B, 0xB9, 0xD6, 0x97};
byte ip[] = {169, 254, 12, 249};
byte gateway[] = {169, 254, 12, 1};
byte subnet[] = {255, 255, 0, 0};
EthernetServer LAN_Serv = EthernetServer(port);
EthernetClient LAN_Client;

#define Buf_Size 10 // размер буфера

#define Data_Size 4 // размер данных в байтах

struct Buf
{
  byte _Instruction;
  byte _Data[Data_Size];
};

struct Mes // структура сообщения
{
  byte _Begin;
  byte _PCB_Address;
  byte _Instruction;
  byte _Data[Data_Size];
  byte _CRC;
  byte _End;
};

// Буферы представлены циклическими очередями
Buf Buf_In[Buf_Size];
Buf Buf_Out[Buf_Size];
unsigned int h1;
unsigned int t1;
unsigned int h2;
unsigned int t2;

void push_in(Buf X)
{
  Buf_In[t1] = X;
  t1 = (t1+1) % Buf_Size;
}

Buf pop_in()
{
  return Buf_In[h1];
  h1 = (h1+1) % Buf_Size;
}

void push_out(Buf X)
{
  Buf_Out[t2] = X;
  t2 = (t2+1) % Buf_Size;
}

Buf pop_out()
{
  return Buf_Out[h2];
  h2 = (h2+1) % Buf_Size;
}

byte size_in()
{
  if (h1 <= t1) {return (t1 - h1);} else {return (t1 + Buf_Size - h1);}
}

byte size_out()
{
  if (h2 <= t2) {return (t2 - h2);} else {return (t2 + Buf_Size - h2);}
}

DHT sensor = DHT();

void setup() 
{
  sensor.attach(A0);
  delay(2000);
  h1 = 0;
  t1 = 0;
  h2 = 0;
  t2 = 0;
  EEPROM.write(0, Num_PCB);
  EEPROM.write(1, port/256);
  EEPROM.write(2, port%256);
  EEPROM.write(3, ip[0]);
  EEPROM.write(4, ip[1]);
  EEPROM.write(5, ip[2]);
  EEPROM.write(6, ip[3]);
  EEPROM.write(7, gateway[0]);
  EEPROM.write(8, gateway[1]);
  EEPROM.write(9, gateway[2]);
  EEPROM.write(10, gateway[3]);
  EEPROM.write(11, subnet[0]);
  EEPROM.write(12, subnet[1]);
  EEPROM.write(13, subnet[2]);
  EEPROM.write(14, subnet[3]);
  Ethernet.begin(mac, ip, gateway, subnet);
  LAN_Serv.begin();
}

#define Begin_In 0xAA
#define Begin_Out 0xFA
#define End 0xFF
#define Error 0xF1

Mes Input;
Mes Output;

#define TimeOut 100

void read_instr()
{
  while((LAN_Client.available() == 0)) { }
  Input._Begin = LAN_Client.read();
  unsigned long Time_0 = millis();
  unsigned long Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._PCB_Address = LAN_Client.read();
  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._Instruction = LAN_Client.read();
  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}

  Input._Data[0] = LAN_Client.read();
  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._Data[1] = LAN_Client.read();
  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._Data[2] = LAN_Client.read();
  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._Data[3] = LAN_Client.read();

  Time_0 = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._CRC = LAN_Client.read();
  Time = millis();
  Time = 0;
  while((LAN_Client.available() == 0) && (Time <= TimeOut)) { Time = millis() - Time_0;}
  Input._End = LAN_Client.read();
}

void write_ans()
{
  LAN_Serv.write(Output._Begin);
  LAN_Serv.write(Output._PCB_Address);
  LAN_Serv.write(Output._Instruction);
  
  LAN_Serv.write(Output._Data[0]);
  LAN_Serv.write(Output._Data[1]);
  LAN_Serv.write(Output._Data[2]);
  LAN_Serv.write(Output._Data[3]);

  LAN_Serv.write(Output._CRC);
  LAN_Serv.write(Output._End);
}

byte CRC(Mes X)
{
  return (X._Begin xor X._PCB_Address xor X._Instruction xor X._Data[0] xor X._Data[1]
    xor X._Data[2] xor X._Data[3] xor X._End);
}

bool check()
{
  return ((Input._Begin == Begin_In) && (Input._End == End) && (Input._CRC == CRC(Input)));
}

void loop() 
{
  LAN_Client = LAN_Serv.available();
  if (LAN_Client && (LAN_Client.connected()))
  {
    // Чтение и декодирование новой команды
    if (size_in() <= (Buf_Size-1))
    {
      read_instr();
      Buf Res;
      // таблица кодов ошибок
      if (!check())
      {        
        Res._Instruction = Error;
        if (Input._Begin == Begin_In) { Res._Data[0] = 0x01; } // несовпадение байта начала команды
        if (Input._End == End) { Res._Data[0] = 0x02; } // несовпадение байта конца команды
        if (Input._CRC == CRC(Input)) { Res._Data[0] = 0x03; } // несовпадение байта CRC
      }
      else
      {
        if ((Input._Instruction != 0x1C) && (Input._Instruction != 0x2C) && (Input._Instruction != 0x3C)
          && (Input._Instruction != 0x4C) && (Input._Instruction != 0x5C) && (Input._Instruction != 0x6C)
          && (Input._Instruction != 0x7C) && (Input._Instruction != 0x8C))
        {
          Res._Instruction = Error;
          Res._Data[0] = 0x04; 
        }
        else
        {
          Res._Instruction = Input._Instruction;
          Res._Data[0] = Input._Data[0];
          Res._Data[1] = Input._Data[1];
          Res._Data[2] = Input._Data[2];
          Res._Data[3] = Input._Data[3];
        }
      }
      if (Input._PCB_Address == Num_PCB) {push_in(Res);}
    }
    // Выбор и исполнение декодированной команды
    if ((size_in() != 0) && (size_out() <= (Buf_Size-1)))
    {
      Buf Com = pop_in();
      switch (Com._Instruction)
      {
        case 0x1C: // изменение адреса платы
        {
          Num_PCB = Com._Data[0];
          EEPROM.write(0, Num_PCB);
          Buf Res;
          Res._Instruction = 0x1C;
          Res._Data[0] = 0xDD;
          push_out(Res);
        }
        break;
        case 0x2C: // изменение номера порта
        {
          int port1 = Com._Data[0];
          int port2 = Com._Data[1];
          port = 256*port1 + port2;
          EEPROM.write(1, Com._Data[0]);
          EEPROM.write(2, Com._Data[1]);
          LAN_Serv = EthernetServer(port);
          Ethernet.begin(mac, ip, gateway, subnet);
          LAN_Serv.begin();
          Buf Res;
          Res._Instruction = 0x2C;
          Res._Data[0] = 0xDD;
          push_out(Res);
        }
        break;
        case 0x3C: // изменение IP-адреса
        {
          ip[0] = Com._Data[0];
          ip[1] = Com._Data[1];
          ip[2] = Com._Data[2];
          ip[3] = Com._Data[3];
          EEPROM.write(3, Com._Data[0]);
          EEPROM.write(4, Com._Data[1]);
          EEPROM.write(5, Com._Data[2]);
          EEPROM.write(6, Com._Data[3]);
          Ethernet.begin(mac, ip, gateway, subnet);
          LAN_Serv.begin();
          Buf Res;
          Res._Instruction = 0x3C;
          Res._Data[0] = 0xDD;
          push_out(Res);
        }
        break;
        case 0x4C: // изменение адреса шлюза
        {
          gateway[0] = Com._Data[0];
          gateway[1] = Com._Data[1];
          gateway[2] = Com._Data[2];
          gateway[3] = Com._Data[3];
          EEPROM.write(7, Com._Data[0]);
          EEPROM.write(8, Com._Data[1]);
          EEPROM.write(9, Com._Data[2]);
          EEPROM.write(10, Com._Data[3]);
          Ethernet.begin(mac, ip, gateway, subnet);
          LAN_Serv.begin();
          Buf Res;
          Res._Instruction = 0x4C;
          Res._Data[0] = 0xDD;
          push_out(Res);
        }
        break;
        case 0x5C: // изменение маски подсети
        {
          subnet[0] = Com._Data[0];
          subnet[1] = Com._Data[1];
          subnet[2] = Com._Data[2];
          subnet[3] = Com._Data[3];
          EEPROM.write(11, Com._Data[0]);
          EEPROM.write(12, Com._Data[1]);
          EEPROM.write(13, Com._Data[2]);
          EEPROM.write(14, Com._Data[3]);
          Ethernet.begin(mac, ip, gateway, subnet);
          LAN_Serv.begin();
          Buf Res;
          Res._Instruction = 0x5C;
          Res._Data[0] = 0xDD;
          push_out(Res);
        }
        break;
        case 0x6C: // чтение версии прошивки
        {
          Buf Res;
          Res._Instruction = 0x6C;
          Res._Data[0] = Version[0];
          Res._Data[1] = Version[1];
          push_out(Res);
        }
        break;
        case 0x7C: // чтение температуры
        {
          Buf Res;
          sensor.update();
          switch (sensor.getLastError())
          {
            case DHT_ERROR_OK:
            {
              Res._Instruction = 0x7C;
              int Temp = sensor.getTemperatureInt();
              Res._Data[0] = Temp / 256;
              Res._Data[1] = Temp % 256;
            }
            break;
            default:
            {
              Res._Instruction = Error;
              Res._Data[0] = 0x05; // ошибка чтения температуры
            }
          }
          push_out(Res);
        }
        break;
        case 0x8C: // чтение влажности
        {
          Buf Res;
          sensor.update();
          switch (sensor.getLastError())
          {
            case DHT_ERROR_OK:
            {
              Res._Instruction = 0x8C;
              int Humid = sensor.getHumidityInt();
              Res._Data[0] = Humid / 256;
              Res._Data[1] = Humid % 256;
            }
            break;
            default:
            {
              Res._Instruction = Error;
              Res._Data[0] = 0x06; // ошибка чтения влажности
            }
          }
          push_out(Res);
        }
        break;
        case Error:
        {
          Buf Res;
          Res._Instruction = Error;
          Res._Data[0] = Com._Data[0];
          push_out(Res);
        }
        break;
      }
    }
    // Запись результатов
    if (size_out() != 0)
    {
      Buf Ready_Com = pop_out();
      Output._Instruction = Ready_Com._Instruction;
      Output._Data[0] = Ready_Com._Data[0];
      Output._Data[1] = Ready_Com._Data[1];
      Output._Data[2] = Ready_Com._Data[2];
      Output._Data[3] = Ready_Com._Data[3];
      Output._Begin = Begin_Out;
      Output._End = End;
      Output._PCB_Address = Num_PCB;
      Output._CRC = CRC(Output);
      write_ans();
    }
  }
}
