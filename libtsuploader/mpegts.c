#include "mpegts.h"

// CRC32 lookup table for polynomial 0x04c11db7
static uint32_t crc_table[256] = {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
        0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
        0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
        0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
        0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
        0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
        0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
        0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
        0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
        0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
        0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
        0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
        0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
        0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
        0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
        0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
        0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
        0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
        0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
        0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
        0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
        0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

static uint8_t h265Aud[] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50};
static uint8_t h264Aud[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xF0};

uint32_t crc32 (uint8_t *data, int len)
{
        register int i;
        uint32_t crc = 0xffffffff;
        
        for (i=0; i<len; i++)
                crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];
        
        return crc;
}

static void initPes(LinkPES *_pPes, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        memset(_pPes, 0, sizeof(LinkPES));
        _pPes->nPts = _nPts * 90; //90000hz, 这里传入的单位是毫秒
        _pPes->pESData = _pData;
        _pPes->nESDataLen = _nDataLen;
        _pPes->nWithPcr = 0;
        return;
}

void LinkInitVideoPES(LinkPES *_pPes, LinkVideoFormat _fmt, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        initPes(_pPes, _pData, _nDataLen, _nPts);
        _pPes->nStreamId = 0xE0;
        _pPes->nWithPcr = 0;
        _pPes->videoFormat = _fmt;
        return;
}

void LinkInitVideoPESWithPcr(LinkPES *_pPes, LinkVideoFormat fmt, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        LinkInitVideoPES(_pPes, fmt, _pData, _nDataLen, _nPts);
        _pPes->nWithPcr = 1;
        return;
}

void LinkInitAudioPES(LinkPES *_pPes, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        initPes(_pPes, _pData, _nDataLen, _nPts);
        _pPes->nStreamId = 0xC0;
        return;
}

void LinkInitPrivateTypePES(LinkPES *_pPes, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        initPes(_pPes, _pData, _nDataLen, _nPts);
        _pPes->nStreamId = 0xBD;
        _pPes->nPrivate = 1;
        return;
}

void NewVideoPES(LinkPES *_pPes, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        initPes(_pPes, _pData, _nDataLen, _nPts);
        _pPes->nStreamId = 0xE0;
        return;
}

// pcr is millisecond
static int writePcrBits(uint8_t *buf, int64_t pcr)
{
        //int64_t pcr_low = pcr % 300, pcr_high = pcr / 300;
        /*
         OPCR_base(i)= ((system_clock_frequencyxt(i))DIV 300)% 2^33
         OPCR_ext(i)= ((system_clock_frequencyxt(i))DIV 1)% 300
         OPCR(i)= OPCR_base(i)x300+OPCR_ext(i)
         */
        int64_t pcr_base = pcr%8589934592; //(pcr/90000 * 27000000/300)%8589934592;
        int64_t pcr_ext = (pcr * 300) % 300; //(pcr/90000 * 27000000) % 300
        
        
        /*
         program_clock_reference_base      33 uimsbf
         reserved                          6bslbf
         program_clock_reference_extension 9 uimsbf
         */
        *buf++ = pcr_base >> 25;
        *buf++ = pcr_base >> 17;
        *buf++ = pcr_base >>  9;
        *buf++ = pcr_base >>  1;
        *buf++ = pcr_base <<  7 | pcr_ext >> 8 | 0x7e;
        *buf++ = pcr_ext;
        
        return 6;
}

static int writeAdaptationFieldJustWithPCR(uint8_t *_pBuf, int64_t _nPcr)
{
        _pBuf[1] = 0x50; //discontinuity_indicator 1bit(0); random_access_indicator 1bit(1); elementary_stream_priority_indicator 1bit(0)
        //PCR_flag 1bit(1); OPCR_flag 1bit(0); splicing_point_flag 1bit(0); transport_private_data_flag 1bit(0); adaptation_field_extension_flag 1bit(0)
        
        int nPcrLen = writePcrBits(&_pBuf[2], _nPcr);
        _pBuf[0] = nPcrLen + 1; //adaptation_field_length 8bit
        return nPcrLen+2;
}

static int writeAdaptationFieldJustWithPravatePadding(uint8_t *_pBuf)
{
        _pBuf[1] = 0x00; //discontinuity_indicator 1bit(0); random_access_indicator 1bit(0); elementary_stream_priority_indicator 1bit(0)
        //PCR_flag 1bit(0); OPCR_flag 1bit(0); splicing_point_flag 1bit(0); transport_private_data_flag 1bit(0); adaptation_field_extension_flag 1bit(0)
        
        memset(_pBuf+2, 0xff, 8);
        _pBuf[0] = 9; //adaptation_field_length 8bit
        return 10;
}

#define AV_RB16(x)                           \
((((const uint8_t*)(x))[0] << 8) |          \
((const uint8_t*)(x))[1])
static void printPts(uint8_t *buf){
        int64_t pts = (int64_t)(*buf & 0x0e) << 29 |
        (AV_RB16(buf+1) >> 1) << 15 |
        AV_RB16(buf+3) >> 1;
        printf("pes pts:%"PRId64"\n", pts/90);
}

static int getPESHeaderJustWithPtsLen(LinkPES *_pPes)
{
        int nLen = 14;
        if (_pPes->videoFormat == LINK_VIDEO_H264) {
                nLen += sizeof(h264Aud);
        } else if (_pPes->videoFormat == LINK_VIDEO_H265) {
                nLen += sizeof(h265Aud);
        }
        return nLen;
}

static int writePESHeaderJustWithPts(LinkPES *_pPes, uint8_t *pData)
{
        uint8_t * pPts = NULL;
        int nRetLen = 0;
        pData[0] = 0; //packet_start_code_prefix 3bit(0x000001)
        pData[1] = 0;
        pData[2] = 1;
        
        pData[3] = _pPes->nStreamId; //stream_id 8bit
        
        int nLen = _pPes->nESDataLen + 8; //PES_packet_length 16bit header[6-13]长度为8
        if (_pPes->videoFormat == LINK_VIDEO_H264) {
                nLen += sizeof(h264Aud);
        } else if (_pPes->videoFormat == LINK_VIDEO_H265) {
                nLen += sizeof(h265Aud);
        }
        if (nLen > 65535) {
                //A value of zero for the PES packet length can be used only when the PES packet payload is a video elementary stream
                assert(_pPes->nStreamId >= 0xE0 && _pPes->nStreamId <= 0xEF);
                pData[4] = 0;
                pData[5] = 0;
        } else {
                pData[4] = nLen / 256;
                pData[5] = nLen % 256;
        }
        
        pData[6] = 0x80; //'10' 2bit(固定的)；PES_scrambling_control 2bit(0);PES_priority 1bit(0);data_alignment_indicator 1bit(0)
        //copyright 1bit(0); original_or_copy 1bit(0)
        pData[7] = 0x80; //PTS_DTS_flags 2bit(2, just PTS); ESCR_flag 1bit(0); ES_rate_flag 1bit(0); DSM_trick_mode_flag 1bit(0)
        //additional_copy_info_flag 1bit(0); PES_CRC_flag 1bit(0); PES_extension_flag 1bit(0)
        pData[8] = 5;//PES_header_data_length 8bit. 剩下长度， 只有pts，pes中pts/dts长度为5
        
        int64_t nPts = _pPes->nPts;
        //pts
        /*
         '0010'              4bit
         PTS [32..30]        3bit
         marker_bit          1bit
         PTS [29..15]        15bit
         marker_bit          1bit
         PTS [14..0]         15bit
         marker_bit          1bit
         */
        pData[9]   = 0x21 | ((nPts >> 29) & 0x0E); //0x21 --> 0010 0001
        pData[10] =  (nPts >>22 & 0xFF);
        pData[11] = 0x01 | ((nPts >> 14 ) & 0xFE);
        pData[12] =  (nPts >> 7 & 0xFF);
        pData[13] = 0x01 | ((nPts << 1 ) & 0xFE);
        nRetLen += 14;
        
        if (_pPes->videoFormat == LINK_VIDEO_H264) {
                memcpy(&pData[nRetLen], h264Aud, sizeof(h264Aud));
                nRetLen += sizeof(h264Aud);
        } else if (_pPes->videoFormat == LINK_VIDEO_H265) {
                memcpy(&pData[nRetLen], h265Aud, sizeof(h265Aud));
                nRetLen += sizeof(h265Aud);
        }
        
        pPts=pData+9; //for debug
        
        //if (pPts != NULL && (_pPes->videoFormat == LINK_VIDEO_H264 || _pPes->videoFormat == LINK_VIDEO_H265))
        //        printPts(pPts);
        
        return nRetLen;
}

int LinkGetPESData(LinkPES *_pPes, int _nCounter, int _nPid, uint8_t *_pData, int _nLen)
{
        if (_pPes->nPos == _pPes->nESDataLen)
                return 0;
        
        int nRetLen = 0;
        int nRemainLen = _pPes->nESDataLen - _pPes->nPos;
        uint8_t * pData = _pData;
        
        int isPaddingBeforePesHdr = 0;
        int nPesHdrLen = 0;
        if (_pPes->nPos == 0) {
                nRetLen = LinkWriteTsHeader(pData, 1, _nCounter, _nPid, LINK_ADAPTATION_JUST_PAYLOAD);
                //https://en.wikipedia.org/wiki/Packetized_elementary_stream
                pData += nRetLen;
                
                if (_pPes->nWithPcr == 1) { //关键帧不可能小于188
                        LinkSetAdaptationFieldFlag(_pData, LINK_ADAPTATION_BOTH);
                        int nAdaLen = writeAdaptationFieldJustWithPCR(pData, _pPes->nPts);
                        nRetLen += nAdaLen;
                        pData += nAdaLen;
                        
                        int nPesHdrLen = writePESHeaderJustWithPts(_pPes, pData);
                        nRetLen += nPesHdrLen;
                        pData += nPesHdrLen;
                } else {
                        nPesHdrLen = getPESHeaderJustWithPtsLen(_pPes);
                        if (nPesHdrLen + nRetLen + _pPes->nESDataLen < 188) {
                                isPaddingBeforePesHdr = 1;
                        } else {
                                writePESHeaderJustWithPts(_pPes, pData);
                                nRetLen += nPesHdrLen;
                                pData += nPesHdrLen;
                                nPesHdrLen = 0;
                        }
                }
                
        }  else {
                nRetLen = LinkWriteTsHeader(pData, 0, _nCounter, _nPid, LINK_ADAPTATION_JUST_PAYLOAD);
        }
        
        int nAdapLen = 0;
        int nReadLen = nRemainLen > _nLen ? _nLen : nRemainLen;
        if (nReadLen + nRetLen + nPesHdrLen >= 188) {
                nReadLen = 188 - nRetLen;
                memcpy(&_pData[nRetLen], _pPes->pESData + _pPes->nPos, nReadLen);
        } else {
                nRetLen += 2; //两字节的adaptation_field
                if (nReadLen + nRetLen + nPesHdrLen > 188) {
                        nReadLen = 188 - nRetLen - nPesHdrLen;
                }
                nAdapLen = 188 - nRetLen - nPesHdrLen - nReadLen;
                
                _pData[nRetLen-2] = nAdapLen + 1; //adaptation_field_lenght不算，但是后面的一个字节算
                _pData[nRetLen-1] = 0x00;
                LinkSetAdaptationFieldFlag(_pData, LINK_ADAPTATION_BOTH); //前面填充ff,然后才是数据
                
                memset(_pData + nRetLen, 0xff, nAdapLen);
                
                nRetLen += nAdapLen; 
                if (isPaddingBeforePesHdr) {
                        nRetLen += writePESHeaderJustWithPts(_pPes, &_pData[nRetLen]);
                }
                memcpy(&_pData[188 - nReadLen], _pPes->pESData + _pPes->nPos, nReadLen);
        }
        
        _pPes->nPos += nReadLen;
        nRetLen += nReadLen;
        assert(nRetLen == 188);
        return 188;
}

void LinkSetAdaptationFieldFlag(uint8_t *_pBuf, int _nAdaptationField)
{
        _pBuf[3] |= (_nAdaptationField << 4);
        return;
}

void LinkWriteContinuityCounter(uint8_t *_pBuf, int _nCounter)
{
        _pBuf[3] |= _nCounter;
}

int LinkWriteTsHeader(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nPid, int _nAdaptationField)
{
        _pBuf[0] = 0x47;
        memset(_pBuf+1, 0, 3);
        if (_nUinitStartIndicator)
                _pBuf[1] |= 0x40;
        
        int nHighPid = _nPid / 256;
        int nLowPid = _nPid % 256;
        _pBuf[1] |= nHighPid;
        _pBuf[2] = nLowPid;
        
        LinkSetAdaptationFieldFlag(_pBuf, _nAdaptationField);
        _pBuf[3] |= _nCount;
        
        return 4;
}

int LinkWriteSDT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField)
{
        int nRetLen = 4;
        LinkWriteTsHeader(_pBuf, _nUinitStartIndicator, _nCount, LINK_SDT_PID, _nAdaptationField);
        _pBuf += 4;
        if (_nUinitStartIndicator) {
                _pBuf[0] = 0;
                nRetLen++;
                _pBuf++; //pointer field
        }
        
        _pBuf[0] = 0x42; //service_id 8bit
        _pBuf[1] = 0xF0;//section_syntax_indicator 1bit(1); reserved_future_use 1bit(1); reserved 2bit(3)
        
        _pBuf[2] = 0x24; //section_length 12bit(prev 4bit)
        
        _pBuf[3] = 0; //transport_stream_id 16bit
        _pBuf[4] = 1;
        
        _pBuf[5] = 0xC1; //reserved 2bit(3);version_number 5bit(0);current_next_indicator 1bit(1)
        
        _pBuf[6] = 0; //section_number 8bit
        _pBuf[7] = 0; //last_section_number 8bit
        
        _pBuf[8] = 0xff; //original_network_id 16bit
        _pBuf[9] = 0x01;
        
        _pBuf[10] = 0xff; //reserved_future_use2 8bit
        
        _pBuf[11] = 0x0; //service_id 16bit
        _pBuf[12] = 0x1;
        
        _pBuf[13] = 0xFF; //reserved_future_use 6bit(63); EIT_schedule_flag 1bit(1);EIT_present_following_flag 1bit(1)
        
        _pBuf[14] = 0x80; //running_status 3bit(4);free_CA_mode 1bit(0)
        
        _pBuf[15] = 0x13; //descriptors_loop_length 12bit(prev 4bit)
        
        _pBuf[16] = 0x48; //descriptor_tag 8bit
        _pBuf[17] = 0x11; //descriptor_length 8bit
        _pBuf[18] = 0x01; //service_type 8bit
        _pBuf[19] = 0x05;// //service_provider_name_length
        memcpy(&_pBuf[20], "qiniu", 5);
        _pBuf[25] = 0x09;//service_name_length 8bt
        memcpy(&_pBuf[26], "service01", 9);
        
        uint32_t c32 = crc32(_pBuf, 35);
        uint8_t *pTmp =  (uint8_t*)&c32;
        _pBuf[35] = pTmp[3];
        _pBuf[36] = pTmp[2];
        _pBuf[37] = pTmp[1];
        _pBuf[38] = pTmp[0];
        
        return 39+nRetLen;
}

int LinkWritePAT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField)
{
        int nRetLen = 4;
        LinkWriteTsHeader(_pBuf, _nUinitStartIndicator, _nCount, LINK_PAT_PID, _nAdaptationField);
        _pBuf += 4;
        if (_nUinitStartIndicator) {
                _pBuf[0] = 0;
                nRetLen++;
                _pBuf++; //pointer field
        }
        _pBuf[0] = 0; //table_id
        
        _pBuf[1] = 0x80; //section_syntax_indicator 1bit(1);zero 1bit(0);reserved 2bit(0); include 4bit section length
        _pBuf[2] = 0x0d; //section_length 12bit
        
        _pBuf[3] = 0x00; //transport_stream_id 16bit
        _pBuf[4] = 0x01;
        
        _pBuf[5] = 0xC1; //reserved 2bit(3); version_number 5bit(0);current_next_indicator 1bit(1)
        
        _pBuf[6] = 0; //section_number 8bit
        _pBuf[7] = 0; //last_section_number 8bit
        
        _pBuf[8] = 0; //program_number 16bit
        _pBuf[9] = 0x01;
        
        _pBuf[10] = 0xF0; //reserved 3bit(7);
        _pBuf[11] = 0x00; //program_map_PID 13bit(4096)
        
        uint32_t c32 = crc32(_pBuf, 12);
        uint8_t *pTmp =  (uint8_t*)&c32;
        _pBuf[12] = pTmp[3];
        _pBuf[13] = pTmp[2];
        _pBuf[14] = pTmp[1];
        _pBuf[15] = pTmp[0];
        
        return 16+nRetLen;
}


int LinkWritePMT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField, int _nVStreamType, int _nAStreamType)
{
        assert(_nVStreamType ||  _nAStreamType);
        int nRetLen = 4;
        int noAOrV = 0;
        LinkWriteTsHeader(_pBuf, _nUinitStartIndicator, _nCount, LINK_PMT_PID, _nAdaptationField);
        _pBuf += 4;
        if (_nUinitStartIndicator) {
                _pBuf[0] = 0;
                _pBuf++; //pointer field
                nRetLen++;
        }
        
        _pBuf[0] = 0x02; //table_id
        
        _pBuf[1] = 0x80; //section_syntax_indicator 1bit;zero 1bit;reserved 2bit; include 4bit section length
        _pBuf[2] = 0x17; //section_length 12bit
        
        _pBuf[3] = 0x00; //program_number 16bit
        _pBuf[4] = 0x01;
        
        _pBuf[5] = 0xC1; //reserved 2bit(3); version_number 5bit(0);current_next_indicator 1bit(1)
        
        _pBuf[6] = 0; //section_number 8bit
        _pBuf[7] = 0; //last_section_number 8bit
        
        _pBuf[8] = 0xE1; //reserved 3bit(14); PCR_PID prev 5bit
        _pBuf[9] = 0x00; //PCR_PID remain 8bit //这个是视频的PID
        
        _pBuf[10] = 0xF0; //reserved 4bit
        _pBuf[11] = 0x00; //program_info_length 12bit(00 mean no descriptor)
        
        if (_nVStreamType != 0) {
                _pBuf[12] = _nVStreamType; //stream_type 8bit STREAM_TYPE_VIDEO_H264
                _pBuf[13] = 0xE1; //reserved 3bit(7), include elementary_PID 5bit
                _pBuf[14] = 0x00; //remain elementary_PID 8bit
                _pBuf[15] = 0xF0; //reserved 4bit, include program_info_length 4bit
                _pBuf[16] = 0x00; //remaint program_info_length 8bit
        } else {
                _pBuf[2] = 0x12; //section_length 12bit
                noAOrV = -5;
        }
        
        if (_nAStreamType != 0) {
                _pBuf[17] = _nAStreamType; //stream_type 8bit STREAM_TYPE_VIDEO_H264
                _pBuf[18] = 0xE1; //reserved 3bit(7), include elementary_PID 5bit
                _pBuf[19] = 0x01; //remain elementary_PID 8bit
                _pBuf[20] = 0xF0; //reserved 4bit, include program_info_length 4bit
                _pBuf[21] = 0x00; //remaint program_info_length 8bit
        } else {
                noAOrV = -5;
                _pBuf[2] = 0x12; //section_length 12bit
        }
        
        uint32_t c32 = crc32(_pBuf, 22);
        uint8_t *pTmp =  (uint8_t*)&c32;
        _pBuf[22] = pTmp[3];
        _pBuf[23] = pTmp[2];
        _pBuf[24] = pTmp[1];
        _pBuf[25] = pTmp[0];
        
        return 26+nRetLen+noAOrV;
}
