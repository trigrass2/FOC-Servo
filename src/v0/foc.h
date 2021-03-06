#ifndef __FOC_H__
#define __FOC_H__

#include "stm32f4xx_adc.h"
#include "stm32f4xx_dac.h"

#include "pid/pid.h"
#include "encoder.h"

#define SQRT3			1.732050807568877f
#define divSQRT3		( 1.0f / SQRT3 )		// 0.57735026918f
#define SQRT3_DIV2		( SQRT3 * 0.5f )		// 0.86602540378f

#define ONE_BY_SQRT3	( SQRT3 )
#define TWO_BY_SQRT3	( 2.0f * SQRT3 )

// -------------------------------------------------------=====================
// ADC1
// Injected Channel:
#define PHASE_A_ADC_CHANNEL		ADC_Channel_8		// PB0
#define VOLT_FDBK_CHANNEL		ADC_Channel_11		// PC1

// Regular Channel:
#define AIN1_ADC_CHANNEL		ADC_Channel_14		// PC4
#define AIN2_ADC_CHANNEL		ADC_Channel_15		// PC5
// -------------------------------------------------------=====================
// ADC2
// Injected Channel:
#define PHASE_B_ADC_CHANNEL		ADC_Channel_9		// PB1
#define AIN0_ADC_CHANNEL		ADC_Channel_12		// PC2
// -------------------------------------------------------=====================
#define SAMPLING_TIME_CK		ADC_SampleTime_15Cycles

#define ARRAYSIZE				( 2 * 4 )
#define ADC1_DR					((uint32_t)0x4001204C)

typedef struct {
	float angle;
	float fSinAngle, fCosAngle;

	int sector;

	float Ia, Ib;
	float Ialpha, Ibeta;

	float Id, Iq;

	float Id_des, Iq_des;

	PID pid_d, pid_q;

	float Vd, Vq;

	float Valpha, Vbeta;
	float Va, Vb, Vc;

	float vbus_voltage;

	int PWM1, PWM2, PWM3;

} MC_FOC, *LP_MC_FOC;

void focInit(LP_MC_FOC lpFocExt);
void mcFocSetAngle(LP_MC_FOC lpFoc, int angle);
void mcFocSetCurrent(LP_MC_FOC lpFoc, float Ia, float Ib);

void mcClark(LP_MC_FOC lpFoc);
void mcPark(LP_MC_FOC lpFoc);
void mcInvPark(LP_MC_FOC lpFoc);
void mcInvClark(LP_MC_FOC lpFoc);

void initDAC(void);
void ADC1_2_IRQHandler( void );

#endif
