#ifndef _GT9XX_CONFIG_H_
#define _GT9XX_CONFIG_H_

/* ***************************PART2:TODO define********************************** */
/* STEP_1(REQUIRED):Change config table. */
/*TODO: puts the config info corresponded to your TP here, the following is just
a sample config, send this config should cause the chip cannot work normally*/
#if 1
#define CTP_CFG_GROUP0 {\
	0x47,0x38,0x04,0x70,0x08,0x05,0x05,0x00,0x01,0x08,0x28,0x05,0x64,0x46,\
	0x03,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8C,\
	0x2C,0x0C,0x78,0x7A,0xEB,0x04,0x00,0x00,0x00,0x23,0x01,0x2C,0x00,0x00,\
	0x00,0x00,0x00,0x03,0x64,0x32,0x00,0x00,0x00,0x64,0x8C,0x94,0xC5,0x02,\
	0x08,0x14,0x00,0x04,0x8D,0x67,0x00,0x87,0x6E,0x00,0x81,0x76,0x00,0x7C,\
	0x7E,0x00,0x77,0x87,0x00,0x77,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0F,0x10,0x12,0x13,0x14,\
	0x28,0x26,0x24,0x22,0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,0x16,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0x56,0x01\
	}
#define GTP_CFG_GROUP0_CHARGER {\
	0x47,0x38,0x04,0x70,0x08,0x05,0x05,0x00,0x01,0x08,0x28,0x05,0x64,0x46,\
	0x03,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8C,\
	0x2C,0x0C,0x78,0x7A,0xEB,0x04,0x00,0x00,0x00,0x23,0x01,0x2C,0x00,0x00,\
	0x00,0x00,0x00,0x03,0x64,0x32,0x00,0x00,0x00,0x64,0x8C,0x94,0xC5,0x02,\
	0x08,0x14,0x00,0x04,0x8D,0x67,0x00,0x87,0x6E,0x00,0x81,0x76,0x00,0x7C,\
	0x7E,0x00,0x77,0x87,0x00,0x77,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0F,0x10,0x12,0x13,0x14,\
	0x28,0x26,0x24,0x22,0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,0x16,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0x56,0x01\
	}
#else
#define CTP_CFG_GROUP0 {\
	0x42,0x38,0x04,0x80,0x07,0x05,0x3D,0x0C,0x20,0x61,0x02,0x0F,0x6F,\
	0x4D,0x33,0x01,0x00,0x32,0x00,0x00,0x28,0x82,0xA0,0xD2,0x18,0x04,\
	0x03,0x00,0x0F,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x40,0x00,0x00,0x00,0x00,0x98,0x00,0x8B,0x00,0x14,0x4C,0x4E,0x7C,\
	0x05,0x00,0x00,0x2A,0x0D,0x43,0x24,0x00,0x02,0x39,0x6C,0xC0,0x94,\
	0x02,0x2A,0x00,0x54,0xB1,0x41,0x9D,0x4A,0x8C,0x52,0x80,0x5B,0x74,\
	0x63,0x6B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x4C,0x3C,0xFF,\
	0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x17,0x64,0x04,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0xEB,0x06,0x32,0x1E,0x1F,0x1D,0x1B,\
	0x1A,0x19,0x18,0x17,0x16,0x15,0x14,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,\
	0x0F,0x10,0x12,0x13,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0x1D,0x1C,0x1B,0x1A,0x19,0x18,0x17,0x15,0x14,0x13,\
	0x12,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,\
	0x30,0x00,0x00,0x00,0xFF,0x14,0x64,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,\
	0x03,0x04,0x32,0x0C,0xE0,0x14,0x50,0x00,0x54,0x10,0x00,0x00,0x00,\
	0x00,0x00,0x15,0xE9,0x01\
}

#define GTP_CFG_GROUP0_CHARGER {\
	0x42,0x38,0x04,0x80,0x07,0x05,0x3D,0x0C,0x20,0x61,0x02,0x0F,0x6F,\
	0x4D,0x33,0x01,0x00,0x32,0x00,0x00,0x28,0x82,0xA0,0xD2,0x18,0x04,\
	0x03,0x00,0x0F,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x40,0x00,0x00,0x00,0x00,0x98,0x00,0x8B,0x00,0x14,0x4C,0x4E,0x7C,\
	0x05,0x00,0x00,0x2A,0x0D,0x43,0x24,0x00,0x02,0x39,0x6C,0xC0,0x94,\
	0x02,0x2A,0x00,0x54,0xB1,0x41,0x9D,0x4A,0x8C,0x52,0x80,0x5B,0x74,\
	0x63,0x6B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x4C,0x3C,0xFF,\
	0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x17,0x64,0x04,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0xEB,0x06,0x32,0x1E,0x1F,0x1D,0x1B,\
	0x1A,0x19,0x18,0x17,0x16,0x15,0x14,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,\
	0x0F,0x10,0x12,0x13,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
	0xFF,0xFF,0xFF,0x1D,0x1C,0x1B,0x1A,0x19,0x18,0x17,0x15,0x14,0x13,\
	0x12,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,\
	0x30,0x00,0x00,0x00,0xFF,0x14,0x64,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,\
	0x03,0x04,0x32,0x0C,0xE0,0x14,0x50,0x00,0x54,0x10,0x00,0x00,0x00,\
	0x00,0x00,0x15,0xE9,0x01\
}
#endif
/* TODO puts your group2 config info here,if need. */
#define CTP_CFG_GROUP1 {\
}

/* TODO puts your group2 config info here,if need. */
#define GTP_CFG_GROUP1_CHARGER {\
}

/* TODO puts your group3 config info here,if need. */
#define CTP_CFG_GROUP2 {\
}

/* TODO puts your group3 config info here,if need. */
#define GTP_CFG_GROUP2_CHARGER {\
}

/* TODO: define your config for Sensor_ID == 3 here, if needed */
#define CTP_CFG_GROUP3 {\
}

/* TODO puts your group3 config info here,if need. */
#define GTP_CFG_GROUP3_CHARGER {\
}

/* TODO: define your config for Sensor_ID == 4 here, if needed */
#define CTP_CFG_GROUP4 {\
}

/* TODO puts your group4 config info here,if need. */
#define GTP_CFG_GROUP4_CHARGER {\
}

/* TODO: define your config for Sensor_ID == 5 here, if needed */
#define CTP_CFG_GROUP5 {\
}

/* TODO puts your group5 config info here,if need. */
#define GTP_CFG_GROUP5_CHARGER {\
}


#endif /* _GT1X_CONFIG_H_ */
