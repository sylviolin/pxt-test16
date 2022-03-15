#include "pxt.h"
#include "MicroBit.h"
#include "MicroBitFile.h"

#ifdef CODAL_CONFIG_H
#define MICROBIT_CODAL 1
#else
#define MICROBIT_CODAL 0
#define target_panic(n) microbit_panic(n)
#define target_wait(n) wait_ms(n)
#endif

#define PXT_PACKET_START        0xFD
#define PXT_PACKET_END          0xFE
#define PXT_CMD_GET_VERSION     0xD0
#define PXT_CMD_SET_FUNC        0xD1
#define PXT_CMD_GET_OBJNUM      0xD2
#define PXT_CMD_GET_DATA        0xD4

#define PXT_BUF_SIZE            64

#define PXT_RET_FW_VERSION		0xE3
#define PXT_RET_OBJNUM			0x46 //0xE4

enum PixSerialPin {
    P0 = 0,
    P1 = 1,
    P2 = 2,
    P8 = 8,
    P12 = 12,
    P13 = 13,
    P14 = 14,
    P15 = 15,
    P16 = 16
};

enum PixFunction {
        //% block="color detection"
        COLOR_DETECTION=1,
        //% block="color codes detection"
        COLOR_LABEL=2,
        //% block="shape detection"
        SHAPE_DETECTION=3,
        //% block="sphere detection"
        SPHERE_DETECTION=4,
        //% block="template matching"
        TEMPLATE=6,
        //% block="keypoint"
        KEYPOINT=8,
        //% block="neural network"
        NEURAL_NETWORK=9,
        //% block="apriltag(16h5)"
        APRILTAG=10,
        //% block="face detection"
        FACE_DETECTION=11,
        //% block="traffic sign detection"
        TRAFFIC_SIGN_DETECTION=12,
        //% block="handwritten digits detection"
        HANDWRITTEN_DIGITS_DETECTION=13,
        //% block="handwritten letters detection"
        HANDWRITTEN_LETTERS_DETECTION=14,
        //% block="remote computing"
        REMOTE_COMPUTING=15,
        //% block="lanes detection"
        LANES_DETECTION=16,
        //% block="digits operation"
        DIGITS_OPERATION=17,
        //% block="simple classifier"
        SIMPLE_CLASSIFIER=18,
        //% block="voice commands"
        VOICE_COMMANDS=19,
        //% block="autonomous driving"
        LANE_AND_SIGN=20        
};
    
enum PixDataField {
        //% block="current function"
        PXTDATA_FUNC_ID=1,
        //% block="return code"
        PXTDATA_RET_CODE,
        //% block="firmware version"
        PXTDATA_VERSION,
        //% block="object type"
        PXTDATA_CLASS_ID,
        //% block="object position x"
        PXTDATA_X,
        //% block="object position y"
        PXTDATA_Y,
        //% block="object width"
        PXTDATA_W,
        //% block="object height"
        PXTDATA_H
};

//using namespace pxt;
//using namespace codal;

//extern MicroBit uBit;

namespace pixetto {
	struct pxt_data {
		// head +4
		uint8_t head;
		uint8_t len;
		uint8_t cmd;
		uint8_t retcode;

		// version +4
		uint32_t version; // 0x00010602

		// object id and bounding box +8
		uint8_t  func_id;
		uint8_t  reserved1;
		uint16_t class_id;
		uint8_t x;
		uint8_t y;
		uint8_t w;
		uint8_t h;

		// extra +32
		union {
			struct {
				float pos_x;
				float pos_y;
				float pos_z;

				float rot_x;
				float rot_y;
				float rot_z;

				float center_x;
				float center_y;
			} apltag; // +32

			struct {
				uint8_t left_x1; 
				uint8_t left_y1; 
				uint8_t left_x2; 
				uint8_t left_y2; 

				uint8_t right_x1; 
				uint8_t right_y1; 
				uint8_t right_x2; 
				uint8_t right_y2; 

				uint8_t sign_x;
				uint8_t sign_y;
				uint8_t sign_w;
				uint8_t sign_h;
			} traffic; // +12

			struct {
				float result;
				char equation[16];
			} math; // +20

			uint8_t bytes[32];
		} extra;

		// End +4
		uint8_t reserved[2];
		uint8_t cksum;
		uint8_t tail;
	};

	MicroBitSerial *serial = nullptr;
	int m_funcid = 0;
	struct pxt_data* m_p;

    bool getPinName(PixSerialPin p, PinName& name) {
      switch(p) {
        case P0: name = MICROBIT_PIN_P0;  return true;
        case P1: name = MICROBIT_PIN_P1;  return true;
        case P2: name = MICROBIT_PIN_P2;  return true;
        case P8: name = MICROBIT_PIN_P8;  return true;
        case P12: name = MICROBIT_PIN_P12; return true;
        case P13: name = MICROBIT_PIN_P13; return true;
        case P14: name = MICROBIT_PIN_P14; return true;
        case P15: name = MICROBIT_PIN_P15; return true;
        case P16: name = MICROBIT_PIN_P16; return true;
      }
      return false;
    }
	
	void addcksum(uint8_t *buf, int len)
	{
		uint8_t sum = 0;
		for (int i = 1; i < len - 2; i++)
			sum += buf[i];
		sum %= 256;
		buf[len - 2] = sum;
	}

	bool verifycksum(uint8_t *buf, int len)
	{
		uint8_t sum = 0;
		for (int i=1; i<len-2; i++)
			sum += buf[i];
		sum %= 256;
		return (sum == buf[len-2]);
	}


	int getdata(uint8_t* buf, int buflen)
	{
		int loop = 0;
		int read_len = 0;

		memset(buf, 0, buflen);

		do {
			read_len = serial->read(buf, 1, ASYNC);
			loop++;
		} while (buf[0] != PXT_PACKET_START && loop < 200000);

		if (read_len == 0 || read_len == MICROBIT_NO_DATA) return 0;

		if (buf[0] == PXT_PACKET_START) {
			read_len = serial->read(&buf[1], 1);
			int n = buf[1];
			if (n <= PXT_BUF_SIZE) {
				read_len = serial->read(&buf[2], n-2);
				if (buf[n-1] == PXT_PACKET_END && verifycksum(buf, n))
					return buf[1];
			}
		}
		return 0;
	}

	//%
	uint32_t pxtGetVersion() {
		uint32_t vers = 0;
		while (vers == 0) {
			serial->clearRxBuffer();
			// Send command
			uint8_t cmd[] = { PXT_PACKET_START, 0x05, PXT_CMD_GET_VERSION, 0, PXT_PACKET_END };
			serial->send(cmd, sizeof(cmd), ASYNC);

			// Get result
			// ex: { FD, 9, E3, 1, 1, 6, 1, F5, FE }

			uint8_t buf[PXT_BUF_SIZE];
			int len = 0;
			if ((len = getdata(buf, PXT_BUF_SIZE)) > 0) {
				if (buf[2] == PXT_CMD_GET_VERSION || buf[2] == PXT_RET_FW_VERSION) {
					m_p = (struct pxt_data*) buf;
					vers = m_p->version;
				}
			}
		}
		return vers;
	}
		
	void pxtWait()
	{
		static bool pxt_is_ready = false;
		while (!pxt_is_ready) {
			if (pxtGetVersion() > 0)
				pxt_is_ready = true;
			else
				uBit.sleep(200);
		}
	}

    //% 
    bool begin(PixSerialPin rx, PixSerialPin tx){
		uint32_t ret = 0;
		PinName txn, rxn;
		uBit.sleep(3000);
		
		if (getPinName(tx, txn) && getPinName(rx, rxn))
		{
			if (serial == nullptr)
				serial = new MicroBitSerial(txn, rxn, 64, 20);
				
			#if MICROBIT_CODAL
			serial->setBaudrate(38400);
			#else
			serial->baud(38400);
			#endif
			//serial->setRxBufferSize(64);
			//serial->setTxBufferSize(32);
			uBit.sleep(1000);
			
			ret = pxtGetVersion();
		}
		return ret;
    }
    
	//%
	void pxtSetFunc(int id)
	{
		pxtWait();

		if (m_funcid == id)
			return;

		uint8_t cmd[] = { PXT_PACKET_START, 0x06, PXT_CMD_SET_FUNC, (uint8_t)id, 0, PXT_PACKET_END };
		addcksum(cmd, sizeof(cmd));
		serial->send(cmd, sizeof(cmd), ASYNC);

		m_funcid = id;
		uBit.sleep(20);
	}
	
	//%
	int pxtAvailable()
	{
		pxtWait();

		uint8_t cmd[] = { PXT_PACKET_START, 0x05, PXT_CMD_GET_OBJNUM, 0, PXT_PACKET_END };
		addcksum(cmd, sizeof(cmd));
		serial->send(cmd, sizeof(cmd), ASYNC);

		uint8_t buf[PXT_BUF_SIZE];
		memset(buf, 0, PXT_BUF_SIZE);

		m_p = (struct pxt_data*) buf;
		int len = 0;

		while ((len = getdata(buf, PXT_BUF_SIZE)) > 0) {
			if (m_p->cmd == PXT_CMD_GET_OBJNUM || m_p->cmd == PXT_RET_OBJNUM) {
				int n = m_p->retcode;
				return n;
			}
		}
		return 0;
	}	
	//%
	bool pxtGetData()
	{
		uint8_t cmd[] = { PXT_PACKET_START, 0x05, PXT_CMD_GET_DATA, 0, PXT_PACKET_END };
		addcksum(cmd, sizeof(cmd));
		serial->send(cmd, sizeof(cmd), ASYNC);
	
		uint8_t buf[PXT_BUF_SIZE];
		memset(buf, 0, PXT_BUF_SIZE);

		m_p = (struct pxt_data*) buf;
		int len;

		while ((len = getdata(buf, PXT_BUF_SIZE)) > 0) {
			if (m_p->cmd == PXT_CMD_GET_DATA && len != 5)
				return true;
		}

		return false;
	}

	//%
	uint32_t getDataField(int field) {
		switch(field) {
			case PXTDATA_FUNC_ID:
				return m_p->func_id;
			case PXTDATA_RET_CODE:
				return m_p->retcode;
			case PXTDATA_VERSION:
				return m_p->version;
			case PXTDATA_CLASS_ID:
				if (m_p->func_id == DIGITS_OPERATION)
					return m_p->extra.math.result;
				else
					return m_p->class_id;
			case PXTDATA_X:
				return m_p->x;
			case PXTDATA_Y:
				return m_p->y;
			case PXTDATA_W:
				return m_p->w;
			case PXTDATA_H:
				return m_p->h;
			default:
				return 0;
		}
	}
}
