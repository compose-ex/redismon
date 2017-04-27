#include <Adafruit_mfGFX.h>
#include <Adafruit_SSD1351_Photon.h>

#define HOSTNAME "sl-eu-lon-2-portal.1.dblayer.com"
#define HOSTPORT 10030
#define HOSTAUTH "MBGJQZDGNQVVEJSX"

#define cs   A2
#define sclk A3
#define mosi A5
#define rst  D5
#define dc   D6

// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

Adafruit_SSD1351 tft = Adafruit_SSD1351(cs, dc, rst);

int waitMillis=1000;

#define VALCNT 64

int off=0;

char *opspersecstr="instantaneous_ops_per_sec:";
int opspersecstrlen=strlen(opspersecstr);
int opspersec[VALCNT];
int lastopspersec;

char *inputkbpsstr="instantaneous_input_kbps:";
int inputkbpsstrlen=strlen(inputkbpsstr);
float inputkbps[VALCNT];
float lastinputkbps;

char *outputkbpsstr="instantaneous_output_kbps:";
int outputkbpsstrlen=strlen(outputkbpsstr);
float outputkbps[VALCNT];
float lastoutputkbps;

char *keyspacehitsstr="keyspace_hits:";
int keyspacehitsstrlen=strlen(keyspacehitsstr);
int keyspacehits[VALCNT];
int lastkeyspacehits;

char *keyspacemissesstr="keyspace_misses:";
int keyspacemissesstrlen=strlen(keyspacemissesstr);
int keyspacemisses[VALCNT];
int lastkeyspacemisses;

#define BUFFLEN 1024

void setup()   {
  Serial.begin(115200);
  Serial.print("hello!");
  tft.begin();
  Serial.println("init");
  Time.zone(1);
  tft.fillScreen(BLACK);
  clear_stats();
}

void clear_stats() {
    for(int i=0;i<VALCNT;i++) {
        opspersec[i]=0;
        inputkbps[i]=0.0;
        outputkbps[i]=0.0;
        keyspacehits[i]=0;
        keyspacemisses[i]=0;
    }
}

TCPClient client;

String lastmsg="";

void sendString(TCPClient client,char *msg) {
  client.print("$");
  client.println(strlen(msg));
  client.println(msg);
}

void update_stats(char *buffer) {
    char *token;
    int state=0; // 0=label, 1=value
    char *label;
    
    token=strtok(buffer,"\n");
    while(token!=NULL) {
        if(token[0]!='#') {
            if(strncmp(token,opspersecstr,opspersecstrlen)==0) {
                lastopspersec=atoi((char *)(token+opspersecstrlen));
                opspersec[off]=lastopspersec;
            } else if(strncmp(token,inputkbpsstr,inputkbpsstrlen)==0) {
                lastinputkbps=atof((char *)(token+inputkbpsstrlen));
                inputkbps[off]=lastinputkbps;
            } else if(strncmp(token,outputkbpsstr,outputkbpsstrlen)==0) {
                lastoutputkbps=atof((char *)(token+outputkbpsstrlen));
                outputkbps[off]=lastoutputkbps;
            } else if(strncmp(token,keyspacehitsstr,keyspacehitsstrlen)==0) {
                lastkeyspacehits=atof((char *)(token+keyspacehitsstrlen));
                keyspacehits[off]=lastkeyspacehits;
            } else if(strncmp(token,keyspacemissesstr,keyspacemissesstrlen)==0) {
                lastkeyspacemisses=atof((char *)(token+keyspacemissesstrlen));
                keyspacemisses[off]=lastkeyspacemisses;
            }
        }
        token=strtok(NULL,"\n");
    }
}

uint8_t getLine(TCPClient client,unsigned char *buffer) {
  char chr;
  uint8_t offset = 0;
  int avail;
  int counter=0;
  
  while((avail=client.available())==0) {
      delay(100);
      counter++;
      if(counter>10) { return -1; }
  }

  while(avail>0) {
      chr = client.read();
      if (chr >= 32 && chr <= 126) {
            buffer[offset++] = chr;
      } else if (chr == '\r') {
            // ignore
      } else if (chr == '\n') {
             buffer[offset++] = '\0';
             return offset; // returning length of string
      }
      avail--;
  }
  return -1;
}

uint8_t getBuffer(TCPClient client,unsigned char *buffer) {
  char chr;
  
  int avail;
  uint16_t expected=0;
  bool gotcount=false;
  
  while(!gotcount) {
        avail=client.available();
        
        while(avail>0 && !gotcount) {
          chr = client.read();
            if (chr >= '0' && chr <= '9') {
                expected *= 10;
                expected += chr - '0';
            } else if (chr == '\r') {
                // ignore
            } else if (chr == '\n') {
                gotcount=true;
            }
            avail--;
        }
    }
  
  uint16_t offset = 0;

  while(offset<expected) {
      
      if(avail==0) {
          avail=client.available();
      }
      
      while(avail>0) {
        chr = client.read();
        if (chr >= 0 && chr <= 126) {
              buffer[offset++] = chr;
        }
        avail--;
      }
    }
    
    buffer[offset++]=0;

    return offset;
}

void render_stats() {
          
    int maxopspersec=0;
    float maxinkbs=0;
    float maxoutkbs=0;
          
    
    for(int i=0;i<VALCNT;i++) {
        if(opspersec[i]>maxopspersec) {maxopspersec=opspersec[i]; }
        if(inputkbps[i]>maxinkbs) { maxinkbs=inputkbps[i]; }
        if(outputkbps[i]>maxoutkbs) { maxoutkbs=outputkbps[i]; }
    }
    
    float r=96.0;
    float sops=r/(float)maxopspersec;
    float sink=r/maxinkbs;
    float sout=r/maxoutkbs;
    
    int i=off+1;
    if(i>=VALCNT) { i=0; }
    
    int loppt=0;
    int likpt=0;
    int lokpt=0;
     
    for(int t=0;t<VALCNT;t++) {
        int oppt=(int)((float)opspersec[i]*sops);
        int ikpt=(int)(inputkbps[i]*sink);
        int okpt=(int)(outputkbps[i]*sout);
    
        if(t==0) {
            loppt=oppt;
            likpt=ikpt;
            lokpt=okpt;
        }
        
        tft.fillRect(t*2,0,2,127,BLACK);
        tft.drawLine(t*2,127-loppt,t*2+1,127-oppt,RED);
        tft.drawLine(t*2,127-likpt,t*2+1,127-ikpt,GREEN);
        tft.drawLine(t*2,127-lokpt,t*2+1,127-okpt,BLUE);
        
        loppt=oppt;
        likpt=ikpt;
        lokpt=okpt;
        
        i=i+1;
        if(i>=VALCNT) { i=0; };
    }
    
    tft.setCursor(0,0);
    tft.setTextColor(RED);
    tft.printf("ops: %d/%d\n",lastopspersec,maxopspersec);
    tft.setTextColor(GREEN);
    tft.printf("ikb: %.1f/%.1f\n",lastinputkbps,maxinkbs);
    tft.setTextColor(BLUE);
    tft.printf("okb: %.1f/%.1f\n",lastoutputkbps,maxoutkbs);
}

void loop() {
    unsigned char buffer[BUFFLEN];
    uint8_t bytesread;

    if(client.connect(HOSTNAME,HOSTPORT)) {

        time_t time=Time.now();

        client.print("*2\r\n");
        sendString(client,"AUTH");
        sendString(client,HOSTAUTH);

        bytesread=getLine(client,buffer);
        
        if(bytesread==4 && strcmp("+OK",(char *)buffer)==0) {

          client.print("*2\r\n");
          sendString(client,"INFO");
          sendString(client,"STATS");

          uint16_t nowread=getBuffer(client,buffer);
          update_stats((char *)buffer);
          render_stats();
          off=off+1;
    
          if(off>=VALCNT) { off=0; }
        } else {
          Serial.printf("Got %d bytes read, trying again\n",bytesread);
        }
        client.stop();
      }

    delay(5000);
}


