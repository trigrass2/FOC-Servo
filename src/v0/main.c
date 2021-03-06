#include "main.h"

#define REG_HOLDING_START   40001
#define REG_HOLDING_NREGS   11

void boardInit(void);

extern volatile uint16_t current_a, current_b, dc_voltage, ai0, ai1;
extern PID pidPos;
extern int16_t enc_delta;

static USHORT usRegHoldingStart = REG_HOLDING_START;
static USHORT usRegHoldingBuf[ REG_HOLDING_NREGS ];

int current_a_offset = 2047;
int current_b_offset = 2047;

int32_t sp_counter = 0;
int32_t counter = 0;

int main_state = 0;

MC_FOC stFoc;

int main(void)
{
	int hall;
	int encoder;
	eMBErrorCode eStatus;
	float Iq_filtered_value = 0, Iq_des_filtered_value = 0, enc_delta_filtered_value;

	SystemInit();
	SystemCoreClockUpdate();

	boardInit();

	focInit( &stFoc );

	eStatus = eMBInit( MB_RTU, 1, 0, 115200, MB_PAR_NONE );
	if( MB_ENOERR == eStatus ) {
		eStatus = eMBEnable();
	}

	usRegHoldingBuf[9] = 0;

	uint32_t sum_a = 0, sum_b = 0;
	for( int i = 0; i < 100; i++ ) {
		for( volatile uint32_t i = 0; i < 100000; i++ );
		sum_a += current_a;
		sum_b += current_b;
	}

	current_a_offset = sum_a / 100;
	current_b_offset = sum_b / 100;

	main_state = 1;

	while( 1 ) {
		GPIO_ToggleBits( GPIOA, GPIO_Pin_15 );

		FirstOrderLagFilter( &Iq_des_filtered_value,  stFoc.Iq_des, 0.00005f );
		FirstOrderLagFilter( &Iq_filtered_value,  stFoc.Iq, 0.00005f );
		FirstOrderLagFilter( &enc_delta_filtered_value,  (float)enc_delta, 0.00002f );

		hall = readHallMap();
		encoder = read360uvwWithOffset( (int16_t)usRegHoldingBuf[9] );

		//float dc_current = sqrtf( stFoc.Id * stFoc.Id + stFoc.Iq * stFoc.Iq );
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		usRegHoldingBuf[0] = (int)Iq_des_filtered_value;//(int)stFoc.Iq_des;
		usRegHoldingBuf[1] = (int)Iq_filtered_value;//(int)stFoc.Iq;
		//usRegHoldingBuf[0] = current_a - current_a_offset;
		//usRegHoldingBuf[1] = current_b - current_b_offset;
		//usRegHoldingBuf[1] = 1000 * pidPos.sumError;-( ( 4095 - current_b ) - current_b_offset );
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		usRegHoldingBuf[2] = (int)enc_delta_filtered_value; sp_counter - iEncoderGetAbsPos(); dc_voltage;
		usRegHoldingBuf[3] = ai0 - 2047;
		// Encoder 0 ( rot.angle )
		usRegHoldingBuf[4] = hall;
		usRegHoldingBuf[5] = encoder;
		usRegHoldingBuf[6] = TIM3->CNT;
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Encoder 1 ( abs.pos )
		usRegHoldingBuf[7] = iEncoderGetAbsPos();
		usRegHoldingBuf[8] = iEncoderGetAbsPos()>>16;
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		(void)eMBPoll();
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	}
}

void EXTI9_5_IRQHandler(void) {
	/* Make sure that interrupt flag is set */
	if( SET == EXTI_GetITStatus( EXTI_Line6 ) ) {
		/* Clear interrupt flag */
		EXTI_ClearITPendingBit( EXTI_Line6 );

		if( !main_state ) {
			return;
		}

		uint16_t pinb = GPIO_ReadInputData( GPIOB );
		uint8_t dir = ( pinb & GPIO_Pin_13 ) ? 1 : 0;

		if( dir ) {
			--counter;
		} else {
			++counter;
		}

		sp_counter = counter * 10;
    }
}

/*
 * http://my.execpc.com/~steidl/robotics/first_order_lag_filter
 *
 * new_filtered_value = k * raw_sensor_value + (1 - k) * old_filtered_value
 *
 * k = 1 - e^ln(1 - Frac) * Ts / Tr
 *
 * Input #1: Maximum Response Time (Tr)
 * This is the (maximum) amount of time (in seconds) between the instant the raw sensor value changes and the instant the filtered sensor value reflects that change.
 *
 * Input #2: Sampling Period (Ts)
 * This is the amount of time (in seconds) between one reading of the raw sensor value and the next.
 *
 * Input #3: Desired Fraction of Change (Frac)
 * This is the (minimum) accuracy with which a change in the raw sensor value (at time t) will be reflected in the filtered sensor value (at or before time t + Tr).
 *
 */
void FirstOrderLagFilter( float *filtered_value, float raw_sensor_value, float k )
{
	*filtered_value = k * raw_sensor_value + ( 1 - k ) * (*filtered_value);
}

eMBErrorCode eMBRegHoldingCB
(
	UCHAR * pucRegBuffer, USHORT usAddress,
	USHORT usNRegs, eMBRegisterMode eMode
)
{
	eMBErrorCode eStatus = MB_ENOERR;
	int iRegIndex;

	if( ( usAddress >= REG_HOLDING_START ) &&
		( usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS )
	) {
		iRegIndex = ( int )( usAddress - usRegHoldingStart );
		switch ( eMode ) {
		case MB_REG_READ:
        	while( usNRegs > 0 ) {
        		*pucRegBuffer++ = ( UCHAR ) ( usRegHoldingBuf[iRegIndex] >> 8 );
        		*pucRegBuffer++ = ( UCHAR ) ( usRegHoldingBuf[iRegIndex] & 0xFF );
        		iRegIndex++;
        		usNRegs--;
        	}
         break;
		case MB_REG_WRITE:
			while( usNRegs > 0 ) {
				usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
				usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
				iRegIndex++;
				usNRegs--;
			}
		 break;
		}
	} else {
		eStatus = MB_ENOREG;
	}
	return eStatus;
}

eMBErrorCode eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress,
                            USHORT usNCoils, eMBRegisterMode eMode )
{
	return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
	return MB_ENOREG;
}

eMBErrorCode eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
	return MB_ENOREG;
}

void boardInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	///////////////////////////////////////////////////////////////////////////
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOA, ENABLE );

	GPIO_StructInit( &GPIO_InitStructure );
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init( GPIOA, &GPIO_InitStructure );

	///////////////////////////////////////////////////////////////////////////
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOB, ENABLE );

	GPIO_StructInit( &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin =
	(
		GPIO_Pin_13 	// MCU_DIR
	);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init( GPIOB, &GPIO_InitStructure );
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	GPIO_StructInit( &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin =
	(
		GPIO_Pin_12 |	// MCU_DO0
		GPIO_Pin_15 	// MCU_BR
	);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;

	GPIO_Init( GPIOB, &GPIO_InitStructure );

	GPIO_ResetBits( GPIOB, GPIO_Pin_15 | GPIO_Pin_12 );

	///////////////////////////////////////////////////////////////////////////
	Configure_PC6();

	///////////////////////////////////////////////////////////////////////////
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOD, ENABLE );

	GPIO_StructInit( &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin =
	(
			GPIO_Pin_11 |	// MCU_CHARGE_RELAY
			GPIO_Pin_10		// MCU_EN_POWER_STAGE
	);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;

	GPIO_Init( GPIOD, &GPIO_InitStructure );

	GPIO_SetBits( GPIOD, GPIO_Pin_10 );
	GPIO_SetBits( GPIOD, GPIO_Pin_11 );
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
}

/* Configure pins to be interrupts */
void Configure_PC6(void)
{
    /* Set variables used */
    GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    /* Enable clock for GPIOD */
    RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOC, ENABLE );
    /* Enable clock for SYSCFG */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_SYSCFG, ENABLE );

    /* Set pin as input */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init( GPIOC, &GPIO_InitStruct );

    /* Tell system that you will use PC6 for EXTI_Line6 */
    SYSCFG_EXTILineConfig( EXTI_PortSourceGPIOC, EXTI_PinSource6 );

    /* PC6 is connected to EXTI_Line6 */
    EXTI_InitStruct.EXTI_Line = EXTI_Line6;
    /* Enable interrupt */
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    /* Interrupt mode */
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Triggers on rising and falling edge */
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    /* Add to EXTI */
    EXTI_Init( &EXTI_InitStruct );

    /* Add IRQ vector to NVIC */
    /* PC6 is connected to EXTI_Line6, which has EXTI9_5_IRQn vector */
    NVIC_InitStruct.NVIC_IRQChannel = EXTI9_5_IRQn;
    /* Set priority */
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x00;
    /* Set sub priority */
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x00;
    /* Enable interrupt */
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    /* Add to NVIC */
    NVIC_Init( &NVIC_InitStruct );
}
