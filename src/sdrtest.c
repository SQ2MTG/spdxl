/*
 * dxlAPRS toolchain
 *
 * Copyright (C) Christian Rabler <oe5dxl@oevsv.at>
 * Modified by SP9SKP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */


#define X2C_int32
#define X2C_index32
#ifndef X2C_H_
#include "X2C.h"
#endif
#define sdrtest_C_
#ifndef osi_H_
#include "osi.h"
#endif
#include <osic.h>
#ifndef aprsstr_H_
#include "aprsstr.h"
#endif
#ifndef sdr_H_
#include "sdr.h"
#endif
#include <stdio.h>
/* test rtl_tcp iq fm demodulator by OE5DXL */
#define sdrtest_MAXCHANNELS 64

#define sdrtest_DEFAULTPOWERSAVE 0

#define sdrtest_SAMPSIZE 32
/* samples per sdr call */

#define sdrtest_TUNESTEP 1000
/* minimal tuning step */

#define sdrtest_TUNEBAND 200000
/* step to minimize freq jumps from inexact pll */

#define sdrtest_DEFAULTMAXWAKE 2000
/* ms stay awake after squelch close */

struct FREQTAB;


struct FREQTAB {
   /*       khz, */
   uint32_t hz;
   uint32_t width;
   uint32_t agc;
   int32_t afc;
   int32_t shiftf;
   char modulation;
};

struct STICKPARM;


struct STICKPARM {
   uint32_t val;
   char ok0;
   char changed;
};

struct SQUELCH;


struct SQUELCH {
   float lev;
   float lp;
   float u1;
   float u2;
   float il;
   float medmed;
   float mutlev;
   int32_t sqsave;
   int32_t wakeness;
   int32_t pcmc;
   uint32_t nexttick;
   uint32_t waterp;
   uint32_t waterdat[16];
};

static int32_t fd;

static uint32_t sndw;

static uint32_t freqc;

static uint32_t midfreq;

static uint32_t lastmidfreq;

static uint32_t downsamp;

static uint32_t mixto;

static uint32_t powersave;

static short sndbuf[1024],sndbufft[1024];

static uint8_t sndbuf8[1024];

static struct sdr_RX rxx[64];

static sdr_pRX prx[65];

static short sampx[64][32];

static struct STICKPARM stickparm[256];

static struct SQUELCH squelchs[65];

static uint32_t iqrate;

static uint32_t samphz;

static uint32_t maxrx;

static uint32_t maxwake;

static char url[1001];

static char port[1001];

static char soundfn[1001];

static char parmfn[1001];

static char verb;

static char reconn;

static char pcm8;

static char nosquelch;

static double offset;

static int wtfno;

time_t oldMTime;

static void Error(char text[], uint32_t text_len)
{
   X2C_PCOPY((void **)&text,text_len);
   osi_WrStr(text, text_len);
   osi_WrStrLn(" error abort", 13ul);
   X2C_ABORT();
   X2C_PFREE(text);
} /* end Error() */


static void card(const char s[], uint32_t s_len, uint32_t * p,
                uint32_t * n, char * ok0)
{
   *ok0 = 0;
   *n = 0UL;
   while ((uint8_t)s[*p]>='0' && (uint8_t)s[*p]<='9') {
      *n = ( *n*10UL+(uint32_t)(uint8_t)s[*p])-48UL;
      *ok0 = 1;
      ++*p;
   }
} /* end card() */

static void cardi(const char s[], uint32_t s_len, uint32_t * p, int32_t * n, char * ok0)
{
   *ok0 = 0;
   *n = 0UL;
   int32_t mn=1;
   if(s[*p]=='-'){
	mn=-1;
	++*p;
   }
   while ((uint8_t)s[*p]>='0' && (uint8_t)s[*p]<='9') {
      *n = ( *n*10UL+(int32_t)(uint8_t)s[*p])-48UL;
      *ok0 = 1;
      ++*p;
   }
    *n*=mn;
} /* end card() */



static void int0(const char s[], uint32_t s_len, uint32_t * p,
                int32_t * n, char * ok0)
{
   char sgn;
   uint32_t c;
   if (s[*p]=='-') {
      sgn = 1;
      ++*p;
   }
   else sgn = 0;
   card(s, s_len, p, &c, ok0);
   if (sgn) *n = -(int32_t)c;
   else *n = (int32_t)c;
} /* end int() */


static void fix(const char s[], uint32_t s_len, uint32_t * p,
                double * x, char * ok0)
{
   double m;
   char sgn;
   if (s[*p]=='-') {
      sgn = 1;
      ++*p;
   }
   else sgn = 0;
   m = 1.0;
   *ok0 = 0;
   *x = 0.0;
   while ((uint8_t)s[*p]>='0' && (uint8_t)s[*p]<='9' || s[*p]=='.') {
      if (s[*p]=='.') m = 0.1;
      else if (m==1.0) {
         *x =  *x*10.0+(double)((uint32_t)(uint8_t)s[*p]-48UL);
      }
      else {
         *x = *x+(double)((uint32_t)(uint8_t)s[*p]-48UL)*m;
         m = m*0.1;
      }
      *ok0 = 1;
      ++*p;
   }
   if (sgn) *x = -*x;
} /* end fix() */


static void Parms(void)
{
   char s[1001];
   uint32_t n;
   uint32_t m;
   int32_t ni;
   char ok0;
   reconn = 0;
   verb = 0;
   pcm8 = 0;
   downsamp = 0UL;
   mixto = 0UL;
   strncpy(url,"127.0.0.1",1001u);
   strncpy(port,"1234",1001u);
   soundfn[0] = 0;
   iqrate = 2048000UL;
   samphz = 16000UL;
   offset = 0.0;
   strncpy(parmfn,"sdrcfg.txt",1001u);
   powersave = 0UL;
   maxrx = 63UL;
   nosquelch = 0;
   /*  watermark:=FALSE; */
   maxwake = 2000UL;
   for (;;) {
      osi_NextArg(s, 1001ul);
      if (s[0U]==0) break;
      if ((s[0U]=='-' && s[1U]) && s[2U]==0) {
         if (s[1U]=='s') {
            osi_NextArg(soundfn, 1001ul);
            pcm8 = 0;
         }
         else if (s[1U]=='S') {
            osi_NextArg(soundfn, 1001ul);
            pcm8 = 1;
         }
         else if (s[1U]=='c') osi_NextArg(parmfn, 1001ul);
         else if (s[1U]=='o') {
            /* offset */
            osi_NextArg(s, 1001ul);
            n = 0UL;
            fix(s, 1001ul, &n, &offset, &ok0);
            if (!ok0) Error(" -o <MHz>", 10ul);
         }
         else if (s[1U]=='d') {
            /* sampelrate to output divide */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &downsamp) || downsamp<1UL) {
               Error(" -p <ratio>", 12ul);
            }
            --downsamp;
         }
         else if (s[1U]=='m') {
            /* downmix to */
            osi_NextArg(s, 1001ul);
            if ((!aprsstr_StrToCard(s, 1001ul,
                &mixto) || mixto<1UL) || mixto>2UL) {
               Error(" -m <1..2>", 11ul);
            }
         }
         else if (s[1U]=='a') {
            /* maximal active rx */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &maxrx)) {
               Error(" -a <number>", 13ul);
            }
         }
         else if (s[1U]=='t') {
            osi_NextArg(s, 1001ul); /* url */
            n = 0UL;
            while ((n<1000UL && s[n]) && s[n]!=':') {
               if (n<1000UL) url[n] = s[n];
               ++n;
            }
            if (n>1000UL) n = 1000UL;
            url[n] = 0;
            if (s[n]==':') {
               m = 0UL;
               ++n;
               while ((n<1000UL && s[n]) && m<1000UL) {
                  port[m] = s[n];
                  ++n;
                  ++m;
               }
               if (m>1000UL) m = 1000UL;
               port[m] = 0;
            }
         }
         else if (s[1U]=='r') {
            /* sampelrate */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &samphz) || samphz>192000UL) {
               Error(" -r <Hz>", 9ul);
            }
         }
         else if (s[1U]=='i') {
            /* iq sampelrate */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul,
                &iqrate) || iqrate!=1024000UL && (iqrate<2048000UL || iqrate>2500000UL)
                ) Error(" -i <Hz> 2048000 or 1024000", 28ul);
         }
         else if (s[1U]=='v') verb = 1;
         else if (s[1U]=='k') reconn = 1;
         else if (s[1U]=='w') {
            /* maximum wake time */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &maxwake)) {
               Error(" -w <ms>", 9ul);
            }
         }
         else if (s[1U]=='z') {
            /* powersave time */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &powersave)) {
               Error(" -z <ms>", 9ul);
            }
            nosquelch = 0;
         }
         else if (s[1U]=='Z') {
            /* powersave time */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &powersave)) {
               Error(" -Z <ms>", 9ul);
            }
            nosquelch = 1;
         }
         else if (s[1U]=='n') {
            /* powersave time */
            osi_NextArg(s, 1001ul);
            if (!aprsstr_StrToCard(s, 1001ul, &wtfno)) {
               Error(" -n <number>", 9ul);
            }
            nosquelch = 0;
         }
         else {
            if (s[1U]=='h') {
               osi_WrStrLn("", 1ul);
               osi_WrStrLn("AM/FM/SSB Multirx from rtl_tcp (8 bit IQ via tcpip) to audio channel(s) 8/16 bit PCM", 85ul);
               osi_WrStrLn(" -a <number>         maximum active rx to limit cpu load, if number is reached,", 80ul);
               osi_WrStrLn("                      no more inactive rx will listen to become active", 71ul);
               osi_WrStrLn(" -c <configfilename> read channels config from file (sdrcfg.txt)", 65ul);
               osi_WrStrLn(" -d <ratio>          downsample output (1)",
                43ul);
               osi_WrStrLn(" -h                  help", 26ul);
               osi_WrStrLn(" -i <kHz>            input sampelrate kHz 1024000 or 2048000..2500000 (2048000)", 80ul);
               osi_WrStrLn("                      if >2048000, AM/FM-IF-width will increase proportional", 77ul);
               osi_WrStrLn(" -k                  keep connection", 37ul);
               osi_WrStrLn(" -m <audiochannels>  mix up/down all rx channels to 1 or 2 audiochannels (mono/stereo)", 87ul);
               osi_WrStrLn("                      for 2 channels the rx audios will be arranged from left to right", 87ul);
               osi_WrStrLn(" -o <mhz>            offset for entered frequencies if Converters are used", 75ul);
               osi_WrStrLn(" -p <cmd> <value>    send rtl_tcp parameter, ppm, tunergain ...", 64ul);
               osi_WrStrLn(" -r <Hz>             output sampelrate Hz for all channels 8000..192000 (16000)", 80ul);
               osi_WrStrLn(" -s <soundfilename>  16bit signed n-channel sound stream/pipe", 62ul);
               osi_WrStrLn(" -S <soundfilename>  8bit unsigned n-channel sound stream/pipe", 63ul);
               osi_WrStrLn(" -t <url:port>       connect rtl_tcp server (127.0.0.1:1234)", 61ul);
               osi_WrStrLn(" -v                  show rssi (dB) and afc (khz)", 50ul);
               osi_WrStrLn(" -w <ms>             max stay awake (use CPU) time after squelch close (2000)", 78ul);
               osi_WrStrLn(" -z <ms>             sleep time (no cpu) for inactive rx if squelch closed (-z 100)", 84ul);
               osi_WrStrLn(" -Z <ms>             same but fast open with no audio quieting for sending", 75ul);
               osi_WrStrLn("                      to decoders and not human ears", 53ul);
	       osi_WrStrLn(" -n <number>         save waterfall to /tmp/wtf<number>.bin", 60ul);
               osi_WrStrLn("example: -m 1 -d 2 -S /dev/dsp -t 127.0.0.1:1234 -p 5 72 -p 8 1 -v", 67ul);
               osi_WrStrLn("", 1ul);
               osi_WrStrLn("config file: (re-read every some seconds and may be modified any time)", 71ul);
               osi_WrStrLn("  # comment", 12ul);
               osi_WrStrLn("  p <cmd> <value>  rtl_tcp parameter like \'p 5 50\' ppm, \'p 8 1\' autogain on", 76ul);
               osi_WrStrLn("  f <mhz> <AFC-range> <squelch%> <lowpass%>  <IF-width>  FM Demodulator", 72ul);
               osi_WrStrLn("  a <mhz>  0          <squelch%> <lowpass%>  <IF-width>  AM Demodulator", 72ul);
               osi_WrStrLn("  u <mhz> <IF-shift>   0         <agc speed> <IF-width>  USB Demodulator", 73ul);
               osi_WrStrLn("  l same for LSB", 17ul);
               osi_WrStrLn("    AFC-range in +-kHz, offset to AFC +- in Hz, Squelch 0 off, 100 open, 70 may do", 83ul);
               osi_WrStrLn("    audio lowpass in % Nyquist frequ. of output sampelrate, 0 is off", 69ul);
               osi_WrStrLn("    IF-width 3000 6000 12000 24000 48000 96000 192000Hz for low CPU usage", 74ul);
               osi_WrStrLn("    (192000 only with >=2048khz iq-rate), (4th order IIR)", 58ul);
               osi_WrStrLn("    (SSB 8th order IF-IIR), OTHER values with MORE CPU-load (12000 default)", 76ul);
               osi_WrStrLn("", 1ul);
               osi_WrStrLn("  example:", 11ul);
               osi_WrStrLn("    p 5 50", 11ul);
               osi_WrStrLn("    p 8 1", 10ul);
               osi_WrStrLn("    f 438.825   5 -5000  75 70         (afc, offset to afc in Hz, squelch, audio lowpass, 12khz IF)", 100ul);
               osi_WrStrLn("    f 439.275   0  0     0  80 20000   (20khz IF, uses more CPU)", 65ul);
               osi_WrStrLn("    u 439.5001 -700 0  0  600     (USB with 600Hz CW-Filter at 800Hz", 69ul);
               osi_WrStrLn("", 1ul);
               osi_WrStrLn("  will generate 3 channel 16bit PCM stream (up to 64 channels with -z or -Z)", 77ul);
               osi_WrStrLn("  use max. 95% of -i span. Rtl-stick will be tuned to center of the span", 73ul);
               osi_WrStrLn("  rx in center of band will be +-10khz relocated to avoid ADC-DC offset pseudo", 79ul);
               osi_WrStrLn("  carriers, SSB-only will be relocated 10..210khz to avoid inexact tuning steps", 80ul);
               osi_WrStrLn("", 1ul);
               osi_WrStrLn("    f 100.1 0 0 15 96000          (WFM with \"-r 192000 -d 4\" for 1 channnel 48khz", 82ul);
               osi_WrStrLn("", 1ul);
               X2C_ABORT();
            }
            if (s[1U]=='p') {
               osi_NextArg(s, 1001ul);
               if (aprsstr_StrToCard(s, 1001ul, &m) && m<256UL) {
                  osi_NextArg(s, 1001ul);
                  if (aprsstr_StrToInt(s, 1001ul, &ni)) {
                     stickparm[m].val = (uint32_t)ni; /* stick parameter */
                     stickparm[m].ok0 = 1;
                     stickparm[m].changed = 1;
                  }
                  else Error(" -p <cmd> <value>", 18ul);
               }
               else {
                  Error(" -p <cmd> <value>", 18ul);
               }
            }
            else Error("-h", 3ul);
         }
      }
      else Error("-h", 3ul);
   }
   powersave = (powersave*samphz)/32000UL;
   maxwake = (maxwake*samphz)/32000UL;
} /* end Parms() */


static void setparms(char all)
{
   uint32_t i;
   char nl;
   char s[31];
   nl = 1;
   for (i = 0UL; i<=255UL; i++) {
      if (stickparm[i].ok0 && (all || stickparm[i].changed)) {
         sdr_setparm(i, stickparm[i].val);
         if (verb) {
            if (nl) {
               osi_WerrLn("", 1ul);
               nl = 0;
            }
            osi_Werr("parm:", 6ul);
            aprsstr_IntToStr((int32_t)i, 0UL, s, 31ul);
            osi_Werr(s, 31ul);
            osi_Werr(" ", 2ul);
            aprsstr_IntToStr((int32_t)stickparm[i].val, 0UL, s, 31ul);
            osi_WerrLn(s, 31ul);
         }
      }
      stickparm[i].changed = 0;
   } /* end for */
} /* end setparms() */


static void setstickparm(uint32_t cmd, uint32_t value)
{
   if (!stickparm[cmd].ok0 || stickparm[cmd].val!=value) {
      stickparm[cmd].val = value;
      stickparm[cmd].changed = 1;
   }
   stickparm[cmd].ok0 = 1;
} /* end setstickparm() */

#define sdrtest_OFFSET 10000


static void centerfreq(const struct FREQTAB freq[], uint32_t freq_len)
{
   uint32_t max0;
   uint32_t min0;
   uint32_t i;
   int32_t nomid;
   char ssb;
   double rem;
   double fhz;
   double khz;
   midfreq = 0UL;
   i = 0UL;
   max0 = 0UL;
   min0 = X2C_max_longcard;
   while (i<freqc) {
      /*WrStr("rx"); WrInt(i+1, 0); WrInt(freq[i].khz, 0); WrStrLn("kHz"); */
      if (freq[i].hz>max0) max0 = freq[i].hz;
      if (freq[i].hz<min0) min0 = freq[i].hz;
      ++i;
   }
   if (max0>=min0) {
      if (max0-min0>=iqrate){
	 osi_WerrLn("freq span > iq-sampelrate", 26ul);
	exit(0);
    }
      midfreq = (max0+min0)/2UL;
      nomid = X2C_max_longint;
      i = 0UL;
      ssb = 0;
      while (i<freqc) {
         if (labs((int32_t)(freq[i].hz-midfreq))<labs(nomid)) {
            nomid = (int32_t)(freq[i].hz-midfreq);
         }
         if (freq[i].modulation=='s') ssb = 1;
         ++i;
      }
      if (labs(nomid)>10000L) nomid = 0L;
      else if (nomid<0L) nomid = 10000L+nomid;
      else nomid -= 10000L;
      midfreq += (uint32_t)nomid;
      if (ssb && max0-min0<200000UL) {
         midfreq = (midfreq/200000UL)*200000UL+10000UL;
      }
      else midfreq = (midfreq/1000UL)*1000UL;
      i = 0UL;
      while (i<freqc) {
         /*     FILL(ADR(rxx[i]), 0C, SIZE(rxx[0])); */
         prx[i] = &rxx[i];
         khz = 1.0;
         if (iqrate>2048000UL) khz = X2C_DIVL(2.048E+6,(double)iqrate);
         fhz = (double)((int32_t)freq[i].hz-(int32_t)midfreq)*khz;
         rxx[i].df = (uint32_t)((int32_t)X2C_TRUNCI(fhz,X2C_min_longint,X2C_max_longint)/1000L);
         rem = fhz-(double)(((int32_t)X2C_TRUNCI(fhz,X2C_min_longint,X2C_max_longint)/1000L)*1000L);
         rxx[i].dffrac = (uint32_t)X2C_TRUNCC(X2C_DIVL(rem,khz)+0.5,0UL, X2C_max_longcard);
         /*      rxx[i].df:=(freq[i].hz DIV 1000 - midfreq DIV 1000); */
         /*      rxx[i].dffrac:=(freq[i].hz-rxx[i].df*1000) MOD 1000; */
         /*WrInt(nomid, 15);WrInt(midfreq, 15);WrInt(freq[i].hz, 15);
                WrInt(rxx[i].df, 15); WrInt(rxx[i].dffrac, 15);
                WrStrLn("n m h d fr"); */
         rxx[i].maxafc = freq[i].afc;
         rxx[i].squelch = squelchs[i].lev!=0.0f;
         rxx[i].width = freq[i].width;
         rxx[i].agc = freq[i].agc;
         rxx[i].modulation = freq[i].modulation;
	 rxx[i].shiftf=freq[i].shiftf;

         ++i;
      }
      prx[i] = 0;
   }
   if (midfreq<20000000UL) osi_WerrLn("no valid frequency", 19ul);
   else if (midfreq!=lastmidfreq) {
      setstickparm(1UL, midfreq);
      /*WrStr("set ");WrInt(midfreq, 0); WrStrLn("kHz"); */
      lastmidfreq = midfreq;
   }
} /* end centerfreq() */


static void skip(const char s[], uint32_t s_len, uint32_t * p)
{
   while (s[*p] && s[*p]==' ') ++*p;
} /* end skip() */


// send freq table
static void updateChanT(){
    
   int i;
   char tmp[15],tmp1[25];

   sndbufft[0]=0;
   strcat(sndbufft,"9 S K P ");

   while (prx[i]) {
      sprintf(tmp,"%02i%06li",i+1,midfreq/1000+rxx[i].df);
      tmp1[0]=tmp[0];
      tmp1[1]='0';
      tmp1[2]=tmp[1];
      tmp1[3]='0';
      tmp1[4]=tmp[2];
      tmp1[5]='0';
      tmp1[6]=tmp[3];
      tmp1[7]='0';
      tmp1[8]=tmp[4];
      tmp1[9]='0';
      tmp1[10]=tmp[5];
      tmp1[11]='0';
      tmp1[12]=tmp[6];
      tmp1[13]='0';
      tmp1[14]=tmp[7];
      tmp1[15]='0';
      tmp1[16]=tmp[8];
      tmp1[17]='0';
      tmp1[18]=0;

//      printf(sndbufft);printf("\n");
      strcat(sndbufft,tmp1);
      ++i;
   }

   osi_WrBin(fd, (char *)sndbufft, 1024u/1u, 1024);
   osi_WrBin(fd, (char *)sndbufft, 1024u/1u, 1024);
   osi_WrBin(fd, (char *)sndbufft, 1024u/1u, 1024);
   osi_WrBin(fd, (char *)sndbufft, 1024u/1u, 1024);

}


int file_is_modified(const char *path) {
    struct stat file_stat;
    float ret;
    int err =  stat(path, &file_stat);

    ret= file_stat.st_mtime > oldMTime;
    oldMTime = file_stat.st_mtime;
    return (int)ret;
}


static void rdconfig(void)
{
   int32_t len;
   int32_t fd0;
   uint32_t sq;
   uint32_t wid;
   uint32_t lpp;
   uint32_t p;
   uint32_t lino;
   uint32_t n;
   uint32_t i;
   int32_t ssbsh;
   int32_t m;
   float sqcor;
   double x;
   char ok0;
   char b[10001];
   char li[256];
   struct FREQTAB freq[64];
   char mo;
   fd0 = osi_OpenRead(parmfn, 1001ul);
   if (fd0>=0L) {
      len = osi_RdBin(fd0, (char *)b, 10001u/1u, 10001UL);
      if ((len>0L && len<10000L) && (uint8_t)b[len-1L]>=' ') b[len-1L] = 0;
      p = 0UL;
      lino = 1UL;
      freqc = 0UL;
      i = 0UL;
      while ((int32_t)p<len) {
         if ((uint8_t)b[p]>=' ') {
            if (i<255UL) {
               li[i] = b[p];
               ++i;
            }
         }
         else if (i>0UL) {
            li[i] = 0;
            i = 0UL;
            skip(li, 256ul, &i);
            mo = X2C_CAP(li[i]);
            if (mo=='P') {
               ++i;
               skip(li, 256ul, &i);
               card(li, 256ul, &i, &n, &ok0);
               if (n>255UL || !ok0) {
                  osi_WerrLn("wrong parameter number", 23ul);
               }
               else {
                  skip(li, 256ul, &i);
                  int0(li, 256ul, &i, &m, &ok0);
                  if (!ok0) osi_WerrLn("wrong value", 12ul);
                  else setstickparm(n, (uint32_t)m);
               }
               i = 0UL;
            }
            else if (((mo=='F' || mo=='A') || mo=='U') || mo=='L') {
               if (mo=='A') freq[freqc].modulation = 'a';
               else if (mo=='F') freq[freqc].modulation = 'f';
               else if (mo=='U') freq[freqc].modulation = 's';
               else if (mo=='L') freq[freqc].modulation = 's';
               else freq[freqc].modulation = 'f';
               ++i;
               skip(li, 256ul, &i);
               fix(li, 256ul, &i, &x, &ok0);
               if (!ok0) osi_WerrLn("wrong MHz", 10ul);
               skip(li, 256ul, &i);
               int0(li, 256ul, &i, &m, &ok0);
               if (!ok0) m = 0L;
               ssbsh = 0L;
               if (mo=='U') {
                  m += 1500L;
                  ssbsh = m;
               }
               else if (mo=='L') {
                  m -= 1500L;
                  ssbsh = m;
               }
               skip(li, 256ul, &i);
               cardi(li, 256ul, &i, &n, &ok0);
               if (!ok0) {
		    n=0;
               }

               skip(li, 256ul, &i);
               card(li, 256ul, &i, &sq, &ok0);
               if (!ok0) sq = 0UL;
               if (sq>200UL) sq = 200UL;
               skip(li, 256ul, &i);
               card(li, 256ul, &i, &lpp, &ok0);
               if (!ok0) lpp = 0UL;
               if (freq[freqc].modulation!='s' && lpp>100UL) lpp = 100UL;
               skip(li, 256ul, &i);
               card(li, 256ul, &i, &wid, &ok0);
               if (!ok0) {
                  if (freq[freqc].modulation=='s') wid = 2800UL;
                  else if (freq[freqc].modulation=='a') wid = 6000UL;
                  else wid = 12000UL;
               }
               if (wid>1000000UL) wid = 1000000UL;


               if (freqc>63UL) osi_WerrLn("freq table full", 16ul);
               else {
                  x = x+offset;
                  if (x<=0.0 || x>=2.147483E+6) {
                     osi_WerrLn("freq out of range", 18ul);
                     x = 0.0;
                  }
                  x = x*1.E+6+(double)ssbsh;

                  freq[freqc].hz = (uint32_t)X2C_TRUNCC(x+0.5,0UL, X2C_max_longcard);
                  freq[freqc].afc = m;
		  freq[freqc].shiftf = n;
                  freq[freqc].width = wid;
                  freq[freqc].agc = lpp;

		 printf ("%u AFC:%u, SH:%i, WI:%u\r\n",freq[freqc].hz,freq[freqc].afc,freq[freqc].shiftf,freq[freqc].width);

		 sqcor=32+(192000/(samphz/1.14)-13.68);

                  squelchs[freqc].lev = X2C_DIVR((float)sq*sqcor,200.0f);
                  if (freq[freqc].modulation=='s') squelchs[freqc].lp = 0.0f;
                  else squelchs[freqc].lp = X2C_DIVR((float)lpp,200.0f);
                  ++freqc;
               }
               i = 0UL;
            }
            else if (mo=='#' || (uint8_t)mo<=' ') i = 0UL;
            else osi_WerrLn("unkown command", 15ul);
            ++lino;
         }
         ++p;
      }
      osic_Close(fd0);
      
   }
   else Error("config file not readable", 25ul);
   centerfreq(freq, 64ul);
   
} /* end rdconfig() */


static void showrssi(void) 
{
   int wfall[] = {238,17,18,19,20,21,27,33,39,28,34,40,46,118,154,190,226,208,214,202,198,197,196};
   unsigned char k,j, i;
   int lvl;
   float lvlf;
   char s[31],tmp[10];
   char fname[30],line[100];
   FILE* stream;


   i = 0UL;
   osi_Werr("\e[1;1H\e[2J",10ul);

   sprintf(tmp,"Param[1]: %i\n",stickparm[1].val);
   osi_Werr(tmp,strlen(tmp)+1);
   sprintf(tmp,"Param[5]: %i\n",stickparm[5].val);
   osi_Werr(tmp,strlen(tmp)+1);
   sprintf(tmp,"Param[8]: %i\n\n",stickparm[8].val);
   osi_Werr(tmp,strlen(tmp)+1);


   if(wtfno>0){
        sprintf(fname,"/tmp/sdr%d.bin",wtfno);
        stream = fopen(fname, "w");
	
	sprintf(line,"1;%i\n",stickparm[1].val);
	fputs(line,stream);
	sprintf(line,"5;%i\n",stickparm[5].val);
	fputs(line,stream);
	sprintf(line,"8;%i\n",stickparm[8].val);
	fputs(line,stream);

   }


   printf("Ch.|Freq kHz|           RSSI          |Shift kHz\n");
   printf("-------------------------------------------------\n");

   while (prx[i]) {
      j = prx[i]->idx;
      sprintf(tmp,"%02i | %li | ",j+1,midfreq/1000+rxx[j].df);
      osi_Werr(tmp,strlen(tmp)+1);
      lvlf=osic_ln((rxx[j].rssit[0]+1.0f)*3.0517578125E-5f)*4.342944819f;
      aprsstr_FixToStr(lvlf, 2UL, s, 31ul);

      osi_Werr(s, 31ul);
      if (squelchs[j].sqsave<=0L) osi_Werr("db", 3ul);
      else osi_Werr("dB", 3ul);


      if(wtfno>0){

        for(k=15;k>0;k--){
	    rxx[j].rssitf[k]=rxx[j].rssitf[k-1];
        }
        rxx[j].rssitf[0]=lvlf;

        sprintf(line,"%02i;%li;%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\r\n",j+1,midfreq/1000+rxx[j].df,
			(unsigned char)rxx[j].rssitf[0],
			(unsigned char)rxx[j].rssitf[1],
			(unsigned char)rxx[j].rssitf[2],
			(unsigned char)rxx[j].rssitf[3],
			(unsigned char)rxx[j].rssitf[4],
			(unsigned char)rxx[j].rssitf[5],
			(unsigned char)rxx[j].rssitf[6],
			(unsigned char)rxx[j].rssitf[7],
			(unsigned char)rxx[j].rssitf[8],
			(unsigned char)rxx[j].rssitf[9],
			(unsigned char)rxx[j].rssitf[10],
			(unsigned char)rxx[j].rssitf[11],
			(unsigned char)rxx[j].rssitf[12],
			(unsigned char)rxx[j].rssitf[13],
			(unsigned char)rxx[j].rssitf[14],
			(unsigned char)rxx[j].rssitf[15]);
	fputs(line,stream);
      }

      lvl=(int)((lvlf-25)/2)-1;
      if(lvl>22) lvl=22;
      if(lvl<0) lvl=0;

      sprintf(tmp," \e[48;5;%im \e[48;5;0m",wfall[lvl]);
    	  osi_Werr(tmp,strlen(tmp));
    
      for(k=1;k<16;k++){
	  if(rxx[j].rssit[k]>22) rxx[j].rssit[k]=22;
	  if(rxx[j].rssit[k]<0) rxx[j].rssit[k]=0;
          sprintf(tmp,"\e[48;5;%im \e[48;5;0m",wfall[(int)rxx[j].rssit[k]]);
    	  osi_Werr(tmp,strlen(tmp));
      }
      for(k=15;k>0;k--){
	rxx[j].rssit[k]=rxx[j].rssit[k-1];
      }
      rxx[j].rssit[1]=(float)lvl;

      /*
      IF NOT nosquelch & rxx[j].squelch THEN
        Werr(" "); FixToStr(squelchs[j].medmed*(1.0/SAMPSIZE), 3, s);
                Werr(s);
      END;
      */
      if (rxx[j].modulation=='f') {
	 osi_Werr(" | ", 4ul);
         aprsstr_IntToStr(rxx[j].afckhz, 0UL, s, 31ul);
         osi_Werr(s, 31ul);
         osi_Werr(" \n", 2ul);
      }
      /*Werr(" "); IntToStr(squelchs[j].wakeness, 0, s); Werr(s); Werr(" ");
                */
      ++i;
   }
   osi_Werr("    \015", 6ul);
   osic_flush();
   if(wtfno>0) fclose(stream);
} /* end showrssi() */

static int32_t sp;

static int32_t sn;

static uint32_t rp;

static uint32_t actch;

static uint32_t ticker;

static uint32_t ix;

static uint32_t tshow;

static uint32_t dsamp;

static char recon;

static int32_t pcm;

static int32_t mixleft;

static int32_t mixright;

static int32_t levdiv2;


static void sendaudio(int32_t pcm0, char pcm80, uint32_t ch)
{
   if (pcm0>32767L) pcm0 = 32767L;
   else if (pcm0<-32767L) pcm0 = -32767L;
   if (pcm80) sndbuf8[sndw] = (uint8_t)((uint32_t)(pcm0+32768L)/256UL);
   else {
      /* code data in watermark */
      /*
          IF watermark THEN
            pcm:=CAST(INTEGER, CAST(SET32, pcm)*SET32{2..15}
                );            (* use bit 1 *)
            WITH squelchs[ch] DO
              IF waterp>0 THEN                                            (* data to send *)
                wb:=waterdat[waterp]
                IF wb*SET32{0}<>SET32{}
                ) THEN INC(pcm, 2) END;            (* send a 1 *)
                wb:=SHIFT(wb, -1);
                waterdat[waterp]:=wb;
                IF wb=SET32{0}) THEN DEC(waterp) END;                     (* next 9 bit word *)
              END;
            END;
          ELSE
      */
      pcm0 = (int32_t)((uint32_t)pcm0&0xFFFEUL);
      /*
          END;
      */
      if (ch==0UL) ++pcm0;
      /* code data in watermark */
      sndbuf[sndw] = (short)pcm0;
   }
   ++sndw;
   if (sndw>1023UL) {
      if (pcm80) osi_WrBin(fd, (char *)sndbuf8, 1024u/1u, sndw);
      else osi_WrBin(fd, (char *)sndbuf, 2048u/1u, sndw*2UL);
      sndw = 0UL;
   }
} /* end sendaudio() */


static void userio(void)
{
   if (file_is_modified(parmfn)){
       rdconfig();
       setparms(recon);
       updateChanT();
   }
   recon = 0;
   if (verb) showrssi();
   
} /* end userio() */


static void schedule(void)
{
   uint32_t j;
   uint32_t i;
   i = 0UL;
   j = 0UL;
   while (j<freqc && i<maxrx) {
      /* first append wake rx */
      if (squelchs[j].wakeness<=0L) {
         /* rx awake */
         squelchs[j].nexttick = ticker;
         prx[i] = &rxx[j];
         ++i;
      }
      ++j;
   }
   j = 0UL;
   while (j<freqc && i<maxrx) {
      /* then sleeping rx until maxrx */
      if (squelchs[j].wakeness>0L && (int32_t)(ticker-squelchs[j].nexttick)
                >=0L) {
         /* rx run */
         squelchs[j].nexttick = ticker;
         prx[i] = &rxx[j];
         ++i;
      }
      ++j;
   }
   prx[i] = 0;
} /* end schedule() */


X2C_STACK_LIMIT(100000l)
extern int main(int argc, char **argv)
{
   int32_t tmp;
   X2C_BEGIN(&argc,argv,1,4000000l,8000000l);
   sdr_BEGIN();
   aprsstr_BEGIN();
   osi_BEGIN();
   midfreq = 0UL;
   lastmidfreq = 0UL;
   tshow = 0UL;
   levdiv2 = 256L;
   memset((char *)rxx,(char)0,sizeof(struct sdr_RX [64]));
   memset((char *)stickparm,(char)0,sizeof(struct STICKPARM [256]));
   memset((char *)squelchs,(char)0,sizeof(struct SQUELCH [65]));
   Parms();
   oldMTime=0;
   actch = 0UL;
   ticker = 0UL;
   prx[0U] = 0;
   for (freqc = 0UL; freqc<=63UL; freqc++) {
      rxx[freqc].samples = (sdr_pAUDIOSAMPLE)sampx[freqc];
      rxx[freqc].idx = freqc;
   } /* end for */
   if (sdr_startsdr(url, 1001ul, port, 1001ul, iqrate, samphz, reconn)) {
      rdconfig();
      updateChanT();
//      file_is_modified(parmfn);
      sndw = 0UL;
      fd = osi_OpenWrite(soundfn, 1001ul);
      if (fd>=0L) {
         recon = 1;
	 rdconfig();
	 updateChanT();

         for (;;) {
            if (tshow==0UL) {
               userio();
               tshow = samphz/32UL;
            }
            else --tshow;
            schedule();
            sn = sdr_getsdr(32UL, prx, 65ul);
            if (sn<0L) {
               if (verb) {
                  if (sn==-2L) {
                     osi_WerrLn("impossible sampelrate conversion", 33ul);
                  }
                  else osi_WerrLn("connection lost", 16ul);
               }
               recon = 1;
               if (!reconn) break;
            }
            else {
               rp = 0UL;
               while (prx[rp]) {
                  ix = prx[rp]->idx;
                  { /* with */
                     struct SQUELCH * anonym = &squelchs[ix];
                     if (anonym->lev==0.0f || prx[rp]->modulation=='s') {
                        squelchs[ix].mutlev = 1.0f;
                     }
                     else {
                        if (anonym->lev<rxx[ix].sqsum) {
                           /* noise */
                           if (anonym->sqsave>0L) {
                              --anonym->sqsave;
                           }
                           else {
                              if (anonym->wakeness<(int32_t)powersave) {
                                 ++anonym->wakeness;
                              }
                              anonym->nexttick = ticker+(uint32_t) anonym->wakeness;
                           }
                        }
                        else {
                           /* squelch open */
                           if (anonym->sqsave<(int32_t)maxwake) {
                              ++anonym->sqsave;
                           }
                           if (anonym->wakeness>-100L) {
                              --anonym->wakeness;
                           }
                        }
                        if (nosquelch || rxx[ix].modulation=='s') {
                           anonym->mutlev = 1.0f;
                        }
                        else {
                           anonym->medmed = anonym->medmed+(rxx[ix].sqsum-anonym->medmed)*0.1f;
                           anonym->mutlev = (anonym->lev-anonym->medmed)*0.3125f;
                           if (anonym->mutlev<0.0f) {
                              anonym->mutlev = 0.0f;
                           }
                           else if (anonym->mutlev>1.0f) {
                              anonym->mutlev = 1.0f;
                           }
                        }
                     }
                  }
                  rxx[ix].sqsum = 0.0f;
                  ++rp;
               }
               tmp = sn-1L;
               sp = 0L;
               if (sp<=tmp) for (;; sp++) {
                  rp = 0UL;
                  while (prx[rp]) {
                     ix = prx[rp]->idx;
                     { /* with */
                        struct SQUELCH * anonym0 = &squelchs[ix];
                        anonym0->pcmc = (int32_t)rxx[ix].samples[sp];
                        if (!nosquelch) {
                           anonym0->pcmc = (int32_t)(short)X2C_TRUNCI((float)anonym0->pcmc*anonym0->mutlev,-32768,32767);
                        }
                        /* lowpass */
                        if (anonym0->lp!=0.0f) {
                           anonym0->u1 = anonym0->u1+(((float)(anonym0->pcmc*2L)-anonym0->u1)-anonym0->il)*anonym0->lp;
                           anonym0->u2 = anonym0->u2+(anonym0->il-anonym0->u2)*anonym0->lp;
                           anonym0->il = anonym0->il+(anonym0->u1-anonym0->u2)*anonym0->lp*2.0f;
                           anonym0->pcmc = (int32_t)X2C_TRUNCI(anonym0->u2, X2C_min_longint,X2C_max_longint);
                        }
                     }
                     /* lowpass */
                     ++rp;
                  }
                  if (dsamp==0UL) {
                     rp = 0UL;
                     while (rp<freqc) {
                        pcm = squelchs[rp].pcmc;
                        squelchs[rp].pcmc = 0L;
                        /* channel mixer */
                        if (mixto==1UL) mixleft += pcm;
                        else if (mixto==2UL) {
                           if (freqc<=1UL) {
                              mixleft = pcm;
                              mixright = pcm;
                           }
                           else {
                              mixleft += pcm*(int32_t)rp;
                              mixright += pcm*(int32_t)((freqc-rp)-1UL);
                           }
                        }
                        /* channel mixer */
                        if (mixto==0UL || rp==0UL) {
                           if (mixto==0UL || freqc==0UL) {
                              sendaudio(pcm, pcm8, rp);
                           }
                           else {
                              sendaudio(mixleft*levdiv2>>8, pcm8, rp);
                              if (mixto==2UL) {
                                 sendaudio(mixright*levdiv2>>8, pcm8, rp);
                              }
                              if (labs(mixleft)>32767L || labs(mixright) >32767L) {
                                 if (levdiv2>20L) {
                                    --levdiv2;
                                 }
                              }
                              else if (levdiv2<256L) {
                                 ++levdiv2;
                              }
                              mixleft = 0L;
                              mixright = 0L;
                           }
                        }
                        ++rp;
                     }
                  }
                  if (dsamp==0UL) dsamp = downsamp;
                  else --dsamp;
                  if (sp==tmp) break;
               } /* end for */
            }
            ++ticker;
         }
      }
      else {
         osi_Werr(soundfn, 1001ul);
         osi_WerrLn(" sound file open error", 23ul);
      }
      if (verb) osi_WerrLn("connection lost", 16ul);
   }
   else osi_WerrLn("not connected", 14ul);
   X2C_EXIT();
   return 0;
}

X2C_MAIN_DEFINITION
