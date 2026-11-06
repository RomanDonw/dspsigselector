/*
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include "dspmodule.h"

#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>

const unsigned short dspmodule_requiredAPIversion = 3;

static void *inleftporta, *inrightporta, *inleftportb, *inrightportb, *outleftport, *outrightport;
static float threshold = 0;
static double fallbacktime = 3, backtime = 2;

unsigned short dspmodule_startup(const DSPLoaderAPI *lapi, int argc, char * const argv[], const char **sysname, const char **dispname)
{
    {
        unsigned int tmp;
        int p;
        while ((p = getopt(argc, argv, "t:f:b:")) != -1)
        {
            switch (p)
            {
                case 't':
                    if (sscanf(optarg, "%f", &threshold) < 1)
                    { lapi->log(DSPLogLevel_Fatal, "failed parsing option -t (required float)"); return 1; }
                    break;

                case 'f':
                    if (sscanf(optarg, "%lf", &fallbacktime) < 1) goto parsedbloptfail;
                    break;

                case 'b':
                    if (sscanf(optarg, "%lf", &backtime) < 1) goto parsedbloptfail;
                    break;

                parsedbloptfail:
                    lapi->log(DSPLogLevel_Fatal, "failed parsing option -%c (required double)", (char)p);
                    return 1;
            }
        }
    }

    if (!(inleftporta = lapi->addport("master_inleft", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating master input left channel"); return 1; }
    if (!(inrightporta = lapi->addport("master_inright", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating master input right channel"); return 1; }

    if (!(inleftportb = lapi->addport("fallback_inleft", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating fallback input left channel"); return 1; }
    if (!(inrightportb = lapi->addport("fallback_inright", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating fallback input right channel"); return 1; }

    if (!(outleftport = lapi->addport("outleft", NULL, DSPPortDirection_Output, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating output left channel"); return 1; }
    if (!(outrightport = lapi->addport("outright", NULL, DSPPortDirection_Output, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating output right channel"); return 1; }

    lapi->log(DSPLogLevel_Info, "threshold: %f\nfallbacktime: %lf seconds\nbacktime: %lf seconds", threshold, fallbacktime, backtime);
    *sysname = "dspsigselector";
    *dispname = "Signal Selector";
    return 0;
}

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{
    float *outleft = lapi->getportbuffer(outleftport, duration);
    if (!outleft) return 0;
    float *outright = lapi->getportbuffer(outrightport, duration);
    if (!outright) return 0;
    
    const float *inlefta = lapi->getportbuffer(inleftporta, duration);
    const float *inrighta = lapi->getportbuffer(inrightporta, duration);

    const float *inleftb = lapi->getportbuffer(inleftportb, duration);
    const float *inrightb = lapi->getportbuffer(inrightportb, duration);

    for (unsigned long i = 0; i < duration; i++)
    {
        double time = (position + i) / (double)rate;
        static double lastsigtime = -1;
        static bool fallbacked = false;
        bool hassig = absf(inlefta ? inlefta[i] : 0) >= threshold && absf(inrighta ? inrighta[i] : 0) >= threshold;

        if (fallbacked)
        {
            static double lastsilencetime = -1;
            if (!hassig || lastsilencetime < 0) lastsilencetime = time;
            else lastsigtime = time;
            if (lastsigtime - lastsilencetime >= backtime) fallbacked = false;
            outleft[i] = inleftb ? inleftb[i] : 0;
            outright[i] = inrightb ? inrightb[i] : 0;
        }
        else
        {
            if (lastsigtime < 0 || hassig) lastsigtime = time;
            else if (time - lastsigtime >= fallbacktime) fallbacked = true;
            outleft[i] = inlefta ? inlefta[i] : 0;
            outright[i] = inrighta ? inrighta[i] : 0;
        }
    }

    return 0;
}
