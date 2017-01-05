// Willian A. Lopes  Eng. computacao
// Firmware para MSP430G2553 para obtencao dos valores de tensao, corrente e radiacao de painel solar
// Projeto desenvolvido com o Prof. Dr. Jean Patric da Costa - UTFPR - PB
//
// Codigo utilizando as referencias internas do ucon (em Vcc e Vss)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <msp430.h>
#include <stdio.h>
#include <string.h>

// CONSTANTES
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ADC_CHANNELS  7	// Canal mais alto a ser utilizado para a ammostragem do ADC

// VARIAVEIS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int VALADC; // Variavel responsavel por receber o valor oriundo do ADC
char CHindex; // Variavel que contem o indice do canal atualmente sendo processado pelo ADC

char TX_string[20]; // String para transmissao via UART
char tx_index; // Indice para transmissao de TX_string

// FUNCOES
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ini_ADC10(void)
{
	ADC10CTL0 = REFOUT + ADC10ON + ADC10IE + ADC10SHT1 + SREF2; //16x clock sample-and-hold + Vref- externa P1.3
	//ADC10CTL0 = REFOUT + ADC10ON + ADC10IE + ADC10SHT1; //64 clock sample-and-hold
	//ADC10CTL0 = ADC10SR + ADC10SHT0 + REFOUT + ADC10ON + ADC10IE;
	// Referencias internas em Vcc e Vss, sample-and-hold 4x ADCClock

 	ADC10CTL1 = INCH0 + INCH1 + INCH2 + SHS0 + CONSEQ0;
 	// Canal mais alto A7, ADC10OSC source, Gatilho por HW pelo TA0.1

	//ADC10AE0 = BIT0 + BIT1 + BIT3 + BIT4 + BIT5 + BIT6 + BIT7;// BIT4;
 	ADC10AE0 = BIT0 + BIT1 + BIT3 + BIT4 + BIT5 + BIT6 + BIT7;// BIT4;
	// P1.0, P1.1, P1.2, P1.5, P1.6 e P1.7

	ADC10CTL0 |= ENC;  // Borda de subida em ENC. Aguardando o gatilho do TA0.1

}

void config_UART(void){

	UCA0CTL1 = UCSWRST;

	UCA0CTL1 |= UCSSEL1;  // Fonte clock: SMCLK ~ 4 MHz | Interface em estado de reset

	UCA0BR0 = 0xA0;  // Para 9600 bps, conforme Tabela 15-4 do User Guide
	UCA0BR1 = 0x01;

	UCA0MCTL = UCBRS1 + UCBRS2;  // UCBRF = 0; UCBRS = 6

	UCA0CTL1 &= ~UCSWRST; // Coloca a interface no modo normal

	IFG2 &= ~UCA0TXIFG;  // Limpa flag de int. de TX, pois o bit eh setado apos reset

	IE2 |= UCA0TXIE; // Habilitando a geracao de int. para RX e TX

}


void ini_TA0(void){

   // Configuracao para Gerar PWM atraves do modulo 1
   // TA0CCR0 a cada 100 ms (10 Hz) e Razao Ciclica de 50%

   // Clock: SMCLK / 8 ~ 250 kHz
   // Modo contador: Up

		TA0CTL = TASSEL1 + MC0 + ID0 + ID1;

		TA0CCTL1 = OUTMOD0 + OUTMOD1 + OUTMOD2 + OUT;

		TA0CCR0 = 62499;  // para periodo de 250 ms

		//TA0CCR1 = 12499;
		//TA0CCR1 = 22350;
		TA0CCR1 = 31249; // periodo de 125 ms
}


void ini_P1_P2(void){

	P1DIR = 0xFF;
	P1OUT = 0;
	P1SEL |= BIT2;   // Ativa P1.2 para TX da UART
	P1SEL2 |= BIT2;

	P2DIR = 0xFF;
    P2OUT = 0;
}


void config_ini(void){

    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

     DCOCTL = CALDCO_16MHZ;   // DCO 16MHz
     BCSCTL1 = CALBC1_16MHZ;
     BCSCTL2 = DIVS1;  // ALTERAR SAPORRA
     BCSCTL3 = XCAP0 + XCAP1;  // Cap LFXT1 ~ 12.5 pF
     while(BCSCTL3 & LFXT1OF);  // Sai do Loop quando LFXT1CLK for estavel
      __enable_interrupt();

}

void cria_msg(void)
{
	//sprintf(TX_string,"MSP%dCH%c@",VALADC,CHindex);
	sprintf(TX_string,"#%d;%04d@",CHindex,VALADC);
	//sprintf(TX_string,";%d;%04d@",4,30);	//http://processors.wiki.ti.com/index.php/Printf_support_in_compiler
	//setar sprintf full (por default esta no minimal)
	IFG2 |= UCA0TXIFG;  // Ativa a transmissao via uart

	//__delay_cycles(20);


}


// RTIS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma vector=USCIAB0TX_VECTOR
__interrupt void RTI_TXD(void){



		if(TX_string[tx_index] == '@'){
			tx_index = 0;
			IFG2 &= ~UCA0TXIFG;  // Limpa a flag de int. para nao entrar mais na RTI
		}else{
			UCA0TXBUF = TX_string[tx_index];
			tx_index++;
		}


	//
	//IFG2 |= UCA0TXIFG;  // Seta a flag de int. de TX para inicar a transmissao


}

// RTI do ADC10
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_RTI(void){

	ADC10CTL0 &= ~ENC;

	VALADC = ADC10MEM;

	cria_msg();

	if(CHindex == 0)	CHindex = ADC_CHANNELS; //Comecar novamente pelo canal mais alto
	else CHindex--;

	ADC10CTL0 |= ENC;  // Borda de subida em ENC

	//IFG2 |= UCA0TXIFG;  // Limpa flag de int. de TX, pois o bit eh setado apos reset
}

// MAIN
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void main(void) {

	int i;
	//for(i=0;i<5000;i++);
	_delay_cycles(10000);

    config_ini();
    ini_P1_P2();
    ini_TA0();
    ini_ADC10();
    config_UART();

    tx_index=0;
    CHindex=ADC_CHANNELS;

	do{

		//_BIS_SR(LPM0_bits + GIE);

	}while(1);


}

