/*
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include "dspmodule.h"

#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>

const unsigned short dspmodule_requiredAPIversion = 4;

static void *inleftporta, *inrightporta, *inleftportb, *inrightportb, *outleftport, *outrightport;
static float threshold = 0;
static double fallbacktime = 3, backtime = 2;

DSPModuleReturnCode dspmodule_startup(const DSPLoaderAPI * restrict lapi, DSPModuleStartupFunctionOptions * restrict opts)
{
    {
        int p;
        while ((p = getopt(opts->argc, opts->argv, "t:f:b:")) != -1)
        {
            switch (p)
            {
                case 't':
                    if (sscanf(optarg, "%f", &threshold) < 1)
                    { lapi->log(DSPLogLevel_Fatal, "failed parsing option -t (required float)"); return DSPModuleReturnCode_Stop; }
                    break;

                case 'f':
                    if (sscanf(optarg, "%lf", &fallbacktime) < 1) goto parsedbloptfail;
                    break;

                case 'b':
                    if (sscanf(optarg, "%lf", &backtime) < 1) goto parsedbloptfail;
                    break;

                parsedbloptfail:
                    lapi->log(DSPLogLevel_Fatal, "failed parsing option -%c (required double)", (char)p);
                    return DSPModuleReturnCode_Stop;
            }
        }
    }

    if (!(inleftporta = lapi->addport("master_inleft", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating master input left channel"); return DSPModuleReturnCode_Stop; }
    if (!(inrightporta = lapi->addport("master_inright", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating master input right channel"); return DSPModuleReturnCode_Stop; }

    if (!(inleftportb = lapi->addport("fallback_inleft", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating fallback input left channel"); return DSPModuleReturnCode_Stop; }
    if (!(inrightportb = lapi->addport("fallback_inright", NULL, DSPPortDirection_Input, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating fallback input right channel"); return DSPModuleReturnCode_Stop; }

    if (!(outleftport = lapi->addport("outleft", NULL, DSPPortDirection_Output, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating output left channel"); return DSPModuleReturnCode_Stop; }
    if (!(outrightport = lapi->addport("outright", NULL, DSPPortDirection_Output, 0)))
    { lapi->log(DSPLogLevel_Fatal, "failed creating output right channel"); return DSPModuleReturnCode_Stop; }

    lapi->log(DSPLogLevel_Info, "threshold: %f\nfallbacktime: %lf seconds\nbacktime: %lf seconds", threshold, fallbacktime, backtime);
    opts->sysname = "dspsigselector";
    opts->dispname = "Signal Selector";
    return 0;
}

DSPModuleReturnCode dspmodule_process(const DSPLoaderAPI * restrict lapi, DSPModuleProcessFunctionOptions * restrict opts)
{
    float *outleft = lapi->getportbuffer(outleftport, opts->duration);
    if (!outleft) return DSPModuleReturnCode_Continue;
    float *outright = lapi->getportbuffer(outrightport, opts->duration);
    if (!outright) return DSPModuleReturnCode_Continue;
    
    const float *inlefta = lapi->getportbuffer(inleftporta, opts->duration);
    const float *inrighta = lapi->getportbuffer(inrightporta, opts->duration);

    const float *inleftb = lapi->getportbuffer(inleftportb, opts->duration);
    const float *inrightb = lapi->getportbuffer(inrightportb, opts->duration);

    for (unsigned long i = 0; i < opts->duration; i++)
    {
        double time = (opts->position + i) / (double)opts->rate;
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

    return DSPModuleReturnCode_Continue;
}
