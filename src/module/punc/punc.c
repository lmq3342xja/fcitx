/***************************************************************************
 *   Copyright (C) 2002~2005 by Yuking                                     *
 *   yuking_net@sohu.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#include "core/module.h"
#include "core/fcitx.h"
#include "core/hook.h"
#include "punc.h"
#include "core/ime.h"
#include "fcitx-config/xdg.h"
#include "fcitx-config/cutils.h"
#include "utils/utils.h"
#include "core/keys.h"
#include <core/ime-internal.h>
#include <core/backend.h>

/**
 * 负责fcitx的标点转换模块。
 */

static WidePunc        *chnPunc = (WidePunc *) NULL;
static boolean PuncInit();
static void ProcessPunc(FcitxKeySym sym, unsigned int state, INPUT_RETURN_VALUE* retVal);

struct FcitxPuncState {
    boolean bUseWidePunc;
    char cLastIsAutoConvert;
    boolean bLastIsNumber;
};

struct FcitxPuncState puncState;

FCITX_EXPORT_API
FcitxModule module = {
    PuncInit,
    NULL
};

boolean PuncInit()
{
    LoadPuncDict();
    RegisterPostInputFilter(ProcessPunc);
    
    puncState.bUseWidePunc = true;
    puncState.cLastIsAutoConvert = '\0';
    puncState.bLastIsNumber = false;
    return true;
}

void ProcessPunc(FcitxKeySym sym, unsigned int state, INPUT_RETURN_VALUE* retVal)
{
    FcitxState* gs = GetFcitxGlobalState();
    char* strStringGet = GetOutputString();
    FcitxIM* im = GetCurrentIM();
    size_t iLen;
    if (puncState.bUseWidePunc) {
        char *pPunc = NULL;

        char *pstr = NULL;
        if (state == KEY_NONE)
            pPunc = GetPunc(sym);

        /* 
         * 在有候选词未输入的情况下，选择第一个候选词并输入标点
         */
        if (pPunc) {
            strStringGet[0] = '\0';
            if (!IsInLegend())
                pstr = im->GetCandWord(0);
            if (pstr)
                strcpy(strStringGet, pstr);
            strcat(strStringGet, pPunc);
            SetMessageCount(gs->messageDown, 0);
            SetMessageCount(gs->messageUp, 0);
            
            *retVal = IRV_PUNC;
        } else if ((IsHotKey(sym, state, FCITX_BACKSPACE) || IsHotKey(sym, state, FCITX_CTRL_H))
                    && puncState.cLastIsAutoConvert) {
            char *pPunc;

            ForwardKey(GetCurrentIC(), FCITX_PRESS_KEY, sym, state);
            pPunc = GetPunc(puncState.cLastIsAutoConvert);
            if (pPunc)
                CommitString(GetCurrentIC(), pPunc);

            *retVal = IRV_DO_NOTHING;
        } else if (IsHotKeySimple(sym, state)) {
            if (IsHotKeyDigit(sym, state))
                puncState.bLastIsNumber = True;
            else {
                puncState.bLastIsNumber = False;
                if (IsHotKey(sym, state, FCITX_SPACE))
                    *retVal = IRV_DONOT_PROCESS_CLEAN;   //为了与mozilla兼容
                else {
                    strStringGet[0] = '\0';
                    if (!IsInLegend())
                        pstr = im->GetCandWord(0);
                    if (pstr)
                        strcpy(strStringGet, pstr);
                    iLen = strlen(strStringGet);
                    SetMessageCount(gs->messageDown, 0);
                    SetMessageCount(gs->messageUp, 0);
                    strStringGet[iLen] = sym;
                    strStringGet[iLen + 1] = '\0';
                    *retVal = IRV_ENG;
                }
            }
        }
    }
    puncState.cLastIsAutoConvert = 0;
}

/**
 * @brief 加载标点词典
 * @param void
 * @return void
 * @note 文件中数据的格式为： 对应的英文符号 中文标点 <中文标点>
 * 加载标点词典。标点词典定义了一组标点转换，如输入‘.’就直接转换成‘。’
 */
int LoadPuncDict (void)
{
    FILE           *fpDict;             // 词典文件指针
    int             iRecordNo;
    char            strText[4 + MAX_PUNC_LENGTH * UTF8_MAX_LENGTH];
    char           *pstr;               // 临时指针
    int             i;

    fpDict = GetXDGFileData(PUNC_DICT_FILENAME, "rt", NULL);

    if (!fpDict) {
        FcitxLog(WARNING, _("Can't open Chinese punc file."));
        return False;
    }

    /* 计算词典里面有多少的数据
     * 这个函数非常简单，就是计算该文件有多少行（包含空行）。
     * 因为空行，在下面会略去，所以，这儿存在内存的浪费现象。
     * 没有一个空行就是浪费sizeof (WidePunc)字节内存*/
    iRecordNo = CalculateRecordNumber (fpDict);
    // 申请空间，用来存放这些数据。这儿没有检查是否申请到内存，严格说有小隐患
    // chnPunc是一个全局变量
    chnPunc = (WidePunc *) malloc (sizeof (WidePunc) * (iRecordNo + 1));

    iRecordNo = 0;

    // 下面这个循环，就是一行一行的读入词典文件的数据。并将其放入到chnPunc里面去。
    for (;;) {
        if (!fgets (strText, (MAX_PUNC_LENGTH * UTF8_MAX_LENGTH + 3), fpDict))
            break;
        i = strlen (strText) - 1;

        // 先找到最后一个字符
        while ((strText[i] == '\n') || (strText[i] == ' ')) {
            if (!i)
                break;
            i--;
        }

        // 如果找到，进行出入。当是空行时，肯定找不到。所以，也就略过了空行的处理
        if (i) {
            strText[i + 1] = '\0';              // 在字符串的最后加个封口
            pstr = strText;                     // 将pstr指向第一个非空字符
            while (*pstr == ' ')
                pstr++;
            chnPunc[iRecordNo].ASCII = *pstr++; // 这个就是中文符号所对应的ASCII码值
            while (*pstr == ' ')                // 然后，将pstr指向下一个非空字符
                pstr++;

            chnPunc[iRecordNo].iCount = 0;      // 该符号有几个转化，比如英文"就可以转换成“和”
            chnPunc[iRecordNo].iWhich = 0;      // 标示该符号的输入状态，即处于第几个转换。如"，iWhich标示是转换成“还是”
            // 依次将该ASCII码所对应的符号放入到结构中
            while (*pstr) {
                i = 0;
                // 因为中文符号都是多字节（这里读取并不像其他地方是固定两个，所以没有问题）的，所以，要一直往后读，知道空格或者字符串的末尾
                while (*pstr != ' ' && *pstr) {
                    chnPunc[iRecordNo].strWidePunc[chnPunc[iRecordNo].iCount][i] = *pstr;
                    i++;
                    pstr++;
                }

                // 每个中文符号用'\0'隔开
                chnPunc[iRecordNo].strWidePunc[chnPunc[iRecordNo].iCount][i] = '\0';
                while (*pstr == ' ')
                    pstr++;
                chnPunc[iRecordNo].iCount++;
            }

            iRecordNo++;
        }
    }

    chnPunc[iRecordNo].ASCII = '\0';
    fclose (fpDict);

    return True;
}

void FreePunc (void)
{
    if (!chnPunc)
        return;

    free (chnPunc);
    chnPunc = (WidePunc *) NULL;
}

/*
 * 根据字符得到相应的标点符号
 * 如果该字符不在标点符号集中，则返回NULL
 */
char           *GetPunc (int iKey)
{
    int             iIndex = 0;
    char           *pPunc;

    if (!chnPunc)
        return (char *) NULL;

    while (chnPunc[iIndex].ASCII) {
        if (chnPunc[iIndex].ASCII == iKey) {
            pPunc = chnPunc[iIndex].strWidePunc[chnPunc[iIndex].iWhich];
            chnPunc[iIndex].iWhich++;
            if (chnPunc[iIndex].iWhich >= chnPunc[iIndex].iCount)
                chnPunc[iIndex].iWhich = 0;
            return pPunc;
        }
        iIndex++;
    }

    return (char *) NULL;
}