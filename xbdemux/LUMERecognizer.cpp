/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <LUMEDefs.h>
#include "include/LUMERecognizer.h"

#define LOG_TAG "LUMERecognizer"
#include <utils/Log.h>

#ifdef DEBUG_LUMERECOGNIZER_CODE_PATH
#define EL_CODEPATH(x, y...) LOGE(x, ##y);
#else
#define EL_CODEPATH(x, y...)
#endif

#define EL_P1(x,y...) //LOGE(x,##y);
#define EL_P2(x,y...) //LOGE(x,##y);


#define MOV_FOURCC(a,b,c,d) ((d<<24)|(c<<16)|(b<<8)|(a))

namespace android {

int64_t LUMERecognizer::SrcCurOffset = 0;

bool LUMERecognizer::is3GPFILE(LUMEStream* source){
    bool oReturn = true;
    source->seek(0, SEEK_SET);

    do{
        int len,id;
        int skipped = 8;
        
        source->read(&len,sizeof(int),1);
        source->read(&id,sizeof(int),1);

        char* ptr = (char*)&id;
        //EL("len:%d;id:%c_%c_%c_%c",len,ptr[0],ptr[1],ptr[2],ptr[3]);
        
        if(len == 1)
        {
            //mpFile->Skip(4,Oscl_File::SEEKCUR);
            source->seek(5,SEEK_CUR);
            source->read(&len,sizeof(int),1);
        }
        else if(len<8) break; // invalid chunk)
        switch(id){
        case MOV_FOURCC('f','t','y','p'): {
            unsigned int tmp;
            // File Type Box (ftyp): 
            // char[4]  major_brand	   (eg. 'isom')
            // int      minor_version	   (eg. 0x00000000)
            // char[4]  compatible_brands[]  (eg. 'mp41')
            // compatible_brands list spans to the end of box
            source->read(&tmp,sizeof(int),1);
            switch(tmp) {
	      //	    case MOV_FOURCC('i','s','o','m'):
            case MOV_FOURCC('3','g','p','1'):
            case MOV_FOURCC('3','g','p','2'):
            case MOV_FOURCC('3','g','2','a'):
            case MOV_FOURCC('3','g','p','3'):
            case MOV_FOURCC('3','g','p','4'):
            case MOV_FOURCC('3','g','p','5'):
                oReturn = false;
            break;
            
            default:
                break;
                
            }
        } break;
        default:
            break;
            
        }
    }while(0);

    return !oReturn;
}

bool LUMERecognizer::isAMRFILE(LUMEStream* source){
    /* FIXME */
    unsigned char amrhdrguid[6]={0x23,0x21,0x41,0x4D,0x52,0x0A};
    unsigned char len[6]= {0};

    source->seek(0, SEEK_SET);
    source->read(&len,6,1);
    if(!memcmp(len,amrhdrguid,6)){
	return true;
    }

    source->seek(0x1b7, SEEK_SET);
    unsigned char amr_of_isom[5]={'5','s','a','m','r'};
    source->read(&len,5,1);
    if(!memcmp(len,amr_of_isom,5)){
	return true;
    }

    return false;
}

//Seems IMY doesn't go through stagefright for now, so actually the fnc is not necessary.
bool LUMERecognizer::isIMYFILE(LUMEStream* source){
    /*imy file hear begin with :*/
    /*BEGIN:IMELADY*/
    unsigned char imyhdrguid[13]={0x42,0x45,0x47,0x49,0x4E,0x3A,
				  0x49,0x4D,0x45,0x4C,0x4F,0x44,0x59};
    unsigned char len[13]= {0};
    
    source->seek(0, SEEK_SET);
    source->read(&len,13,1);
    if(!memcmp(len,imyhdrguid,13)){
	return true;
    }

    return false;
}

bool LUMERecognizer::isSWFFILE(LUMEStream* source){
    /*swf file hear begin with :*/
    /*BEGIN:IMELADY*/
    unsigned char swfhdrguid[3]={0x46,0x57,0x53};
    unsigned char len[3]= {0};
    
    source->seek(0, SEEK_SET);
    source->read(&len,3,1);

    if(!memcmp(len,swfhdrguid,3)){
	return true;
    }

    return false;
}

bool LUMERecognizer::isOGGFILE(LUMEStream* source){
    unsigned char ogghdrguid[4] = {'O', 'g', 'g', 'S'};
    unsigned char len[4]= {0};
    
    source->seek(0, SEEK_SET);
    source->read(len,4,1);

    if(!memcmp(len,ogghdrguid,4))
	return true;

    return false;
}

bool LUMERecognizer::isSystemRingToneFILE(LUMEStream* source){
    if(isOGGFILE(source)){
    }

    return false;
}

bool LUMERecognizer::recognize(const sp<DataSource> &source){
    LUMEStream stream(source.get());

    if(isOGGFILE(&stream)){
	EL_P1("OGG not lume format");
	return false;
    }
#if 0
    if(is3GPFILE(&stream)){
        EL_P1("3GP not lume format");  
        return false;
    }
#endif    
    if(isAMRFILE(&stream)){
        EL_P1("AMR not lume format");
	return false;
    }
    
    if(isIMYFILE(&stream)){
        EL_P1("IMY not lume format");
        return false;
    }
    
    if(isSWFFILE(&stream)){
	EL_P1("SWF not lume format");
	return false;
    }

    stream.seek(0, SEEK_SET);

    return true;
}

}//namespace android
