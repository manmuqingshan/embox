/*==============================================================================
 * Драйвер интерфейса I2C для МК К1921ВК035
 *------------------------------------------------------------------------------
 * ДЦЭ Vostok г.Владивосток 2022г., Лебедев Михаил <micha030201@gmail.com>,
 * ДЦЭ Vostok г.Владивосток 2022г., Войтенко Алексей <alexeyvoytenko.2000@gmail.com>,
 *==============================================================================
 */

#include "I2C_driver.h"

typedef struct {
    uint8_t operations_size;
    uint8_t operations_start;
    I2C_driver_state_t state;
    I2C_driver_operation_t* operations;
} I2C_driver_t;

static I2C_driver_t I2C_driver;

void I2C_driver_init(uint32_t freq) {
    NVIC_EnableIRQ(I2C_IRQn);
    NVIC_SetPriority(I2C_IRQn, 0);

    RCU_APBClkCmd(RCU_APBClk_I2C, ENABLE);
    RCU_APBRstCmd(RCU_APBRst_I2C, ENABLE);

    GPIO_Init_TypeDef gpio_i2c_init_struct;
    GPIO_StructInit(&gpio_i2c_init_struct);
    gpio_i2c_init_struct.Pin = I2C_DRIVER_SCL | I2C_DRIVER_SDA;
    gpio_i2c_init_struct.AltFunc = ENABLE;
    gpio_i2c_init_struct.Out = ENABLE;
    gpio_i2c_init_struct.OutMode = GPIO_OutMode_OD;
    gpio_i2c_init_struct.PullMode = GPIO_PullMode_PU;
    gpio_i2c_init_struct.InMode = GPIO_InMode_Schmitt;
    gpio_i2c_init_struct.Digital = ENABLE;
    GPIO_Init(I2C_DRIVER_PORT, &gpio_i2c_init_struct);

    I2C_FSFreqConfig(freq, SystemCoreClock);
    I2C_Cmd(ENABLE);
    I2C_ITCmd(ENABLE);
    I2C_SlaveCmd(DISABLE);
}

void restart_or_stop() {
    if (++I2C_driver.operations_start == I2C_driver.operations_size) {
        I2C_StopCmd();
        I2C_driver.state = I2C_DRIVER_OK;
    } else {
        I2C_StartCmd();
        I2C_driver.state = I2C_DRIVER_BUSY;
    }
}

void I2C_IRQHandler() {
    switch (I2C_GetState()) {
        case I2C_State_STDONE: /*!< FS мастер - Сформировано состояние СТАРТа */
        case I2C_State_RSDONE: /*!< FS мастер - Сформировано состояние повторного СТАРТа */
            I2C_SetData(I2C_driver.operations[I2C_driver.operations_start].address);
            I2C_driver.state = I2C_DRIVER_BUSY;
            break;
        case I2C_State_MTADPA: /*!< FS мастер передача - Отправлен адрес ведомого, ACK */
            if (I2C_driver.operations[I2C_driver.operations_start].size) {
                I2C_SetData(I2C_driver.operations[I2C_driver.operations_start]
                                .data[I2C_driver.operations[I2C_driver.operations_start].start++]);
                I2C_driver.state = I2C_DRIVER_BUSY;
            } else {
                restart_or_stop();
            }
            break;
        case I2C_State_MTDAPA: /*!< FS мастер передача - Отправлен байт данных, ACK */
            if (I2C_driver.operations[I2C_driver.operations_start].start
                != I2C_driver.operations[I2C_driver.operations_start].size) { // FIXME hard to read with this styleguide
                I2C_SetData(I2C_driver.operations[I2C_driver.operations_start]
                                .data[I2C_driver.operations[I2C_driver.operations_start].start++]);
                I2C_driver.state = I2C_DRIVER_BUSY;
            } else {
                restart_or_stop();
            }
            break;
        case I2C_State_MRADPA: /*!< FS мастер приём - Отправлен адрес ведомого, ACK */
            if (I2C_driver.operations[I2C_driver.operations_start].size == 1) {
                I2C_NACKCmd();
                I2C_driver.state = I2C_DRIVER_BUSY;
            } else if (I2C_driver.operations[I2C_driver.operations_start].size == 0) {
                restart_or_stop();
                I2C_driver.state = I2C_DRIVER_OK;
            } else {
                I2C_driver.state = I2C_DRIVER_BUSY;
            }
            break;
        case I2C_State_MRDAPA: /*!< FS мастер приём - Принят байт данных, ACK */
            if (I2C_driver.operations[I2C_driver.operations_start].start
                != I2C_driver.operations[I2C_driver.operations_start].size) {
                I2C_driver.operations[I2C_driver.operations_start]
                    .data[I2C_driver.operations[I2C_driver.operations_start].start++] = I2C_GetData();
                if (I2C_driver.operations[I2C_driver.operations_start].start
                    == I2C_driver.operations[I2C_driver.operations_start].size - 1) {
                    I2C_NACKCmd();
                }
                I2C_driver.state = I2C_DRIVER_BUSY;
            } else {
                I2C_driver.state = I2C_DRIVER_ERROR;
            }
            break;
        case I2C_State_MRDANA: /*!< FS мастер приём - Принят байт данных, NACK */
            if (I2C_driver.operations[I2C_driver.operations_start].start
                == I2C_driver.operations[I2C_driver.operations_start].size - 1) {
                I2C_driver.operations[I2C_driver.operations_start]
                    .data[I2C_driver.operations[I2C_driver.operations_start].start++] = I2C_GetData();
                restart_or_stop();
            } else {
                I2C_driver.state = I2C_DRIVER_ERROR;
            }
            break;
        case I2C_State_IDLE: /*!< Общий - Idle, нет доступной статусной информации */
        case I2C_State_IDLARL: /*!< FS мастер - Потеря арбитража, переход в режим безадресного ведомого */
        case I2C_State_MTADNA: /*!< FS мастер передача - Отправлен адрес ведомого, NACK */
        case I2C_State_MTDANA: /*!< FS мастер передача - Отправлен байт данных, NACK */
        case I2C_State_MRADNA: /*!< FS мастер приём - Отправлен адрес ведомого, NACK */
        case I2C_State_MTMCER: /*!< FS мастер - Отправлен код мастера, обнаружена ошибка, ACK */
        default:
            I2C_StopCmd();
            I2C_driver.state = I2C_DRIVER_ERROR;
            break;
    }
    I2C_ITStatusClear();
}

// Low-level API:

void I2C_driver_execute(I2C_driver_operation_t* operations, uint8_t size) {
    I2C_driver.operations = operations;
    I2C_driver.operations_size = size;
    I2C_driver.operations_start = 0;
    I2C_driver.state = I2C_DRIVER_BUSY;

    I2C_StartCmd();
}

I2C_driver_state_t I2C_driver_is_done() {
    if (I2C_driver.state == I2C_DRIVER_OK) {
        if (I2C->CTL0_bit.STOP) {
            return I2C_DRIVER_BUSY;
        }
    }
    return I2C_driver.state;
}

// Slightly higher-level API:

void I2C_driver_write(uint8_t address, const uint8_t* data, uint8_t size) {
    I2C_driver_operation_t op;

    op.address = (address << 1) | I2C_DRIVER_WRITE_FLAG;
    op.data = (uint8_t*)data;
    op.size = size;
    op.start = 0;

    I2C_driver_execute(&op, 1);
}

void I2C_driver_read(uint8_t address, uint8_t* data, uint8_t size) {
    I2C_driver_operation_t op;

    op.address = (address << 1) | I2C_DRIVER_READ_FLAG;
    op.data = data;
    op.size = size;
    op.start = 0;

    I2C_driver_execute(&op, 1);
}
