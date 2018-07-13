// 7 bit slave I2C address
const int SLAVE_ADDR = 0x8;

// register address
const int REG_ADDR_MASK = 0x7F;
const int REG_ADDR_RST_FLAG_MASK = 0x80;

// register addresses
const int BYTE_ADDR0 = 0;
const int BYTE_ADDR1 = 1;
const int ZERO_ADDR = 2;
const int CONTROL_ADDR = 3;
const int WORD_ADDR0 = 4;
const int WORD_ADDR1 = 6;
const int BLOCK_ADDR = 8;

// amount of byte to response on SMBus Block read
const int BLOCK_RESP_LENGTH = SLAVE_BUFFER_SIZE - 1;

// register
const uint8_t REG_DEFAULT[] =
  {
    0xA0, //  [0] for byte write/read
    0x50, //  [1] for byte write/read

    0x11, //  [2] reserved for zero mode (no response)

    0x00, //  [3] control

    0x00, //  [4] for word write/read
    0xAA, //  [5] for word write/read
    0x55, //  [6] for word write/read
    0xFD, //  [7] for word write/read

    0x10, //  [8] for block write/read
    0x11, //  [9] for block write/read
    0x12, // [10] for block write/read
    0x13, // [11] for block write/read
    0x14, // [12] for block write/read
    0x15, // [13] for block write/read
    0x16, // [14] for block write/read
    0x17, // [15] for block write/read
    0x18, // [16] for block write/read
    0x19, // [17] for block write/read
    0x1A, // [18] for block write/read
    0x1B, // [19] for block write/read
    0x1C, // [20] for block write/read
    0x1D, // [21] for block write/read
    0x1E, // [22] for block write/read
    0x1F, // [23] for block write/read
    0x20, // [24] for block write/read
    0x21, // [25] for block write/read
};

const int REG_SIZE = sizeof(REG_DEFAULT);

// control states
const uint8_t CONTROL_AUX_MASK = 0x3;
const uint8_t CONTROL_AUX_POS  = 0;
const uint8_t CONTROL_PWR_MASK = 0x3;
const uint8_t CONTROL_PWR_POS  = 2;

// AUX pin states
const uint8_t AUX_STATE_OFF    = 0x00; // aux pin low
const uint8_t AUX_STATE_ON     = 0x01; // aux pin high
const uint8_t AUX_STATE_TOGGLE = 0x02; // toggle pin on every loop() cycle
const uint8_t AUX_STATE_CB     = 0x03; // aux pin high after request
                                       // low after receive

// power states
const uint8_t PWR_STATE_AWAKE  = 0x00; // no sleep mode
const uint8_t PWR_STATE_IDLE   = 0x01; // idle sleep mode
const uint8_t PWR_STATE_DOWN   = 0x02; // power down sleep mode
