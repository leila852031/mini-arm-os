#include <stddef.h>
#include <stdint.h>
#include "reg.h"
#include "threads.h"

/* USART TXE Flag
 * This flag is cleared when data is written to USARTx_DR and
 * set when that data is transferred to the TDR
 */
#define USART_FLAG_TXE	((uint16_t) 0x0080)
#define USART_FLAG_RXNE ((uint16_t) 0x0020)
#define LOCK 1
#define UNLOCK 0

int mutex = UNLOCK;

void usart_init(void)
{
	*(RCC_APB2ENR) |= (uint32_t) (0x00000001 | 0x00000004);
	*(RCC_APB1ENR) |= (uint32_t) (0x00020000);

	/* USART2 Configuration, Rx->PA3, Tx->PA2 */
	*(GPIOA_CRL) = 0x00004B00;
	*(GPIOA_CRH) = 0x44444444;
	*(GPIOA_ODR) = 0x00000000;
	*(GPIOA_BSRR) = 0x00000000;
	*(GPIOA_BRR) = 0x00000000;

	*(USART2_CR1) = 0x0000000C;
	*(USART2_CR2) = 0x00000000;
	*(USART2_CR3) = 0x00000000;
	*(USART2_CR1) |= 0x2000;
}

void print_str(const char *str)
{
	while (*str) {
		while (!(*(USART2_SR) & USART_FLAG_TXE));
		*(USART2_DR) = (*str & 0xFF);
		str++;
	}
}

void print_char(const char *chr)
{
	if (*chr) {
		while (!(*(USART2_SR) & USART_FLAG_TXE));
		*(USART2_DR) = (*chr & 0xFF);
	}
} 

char get_char(void)
{
	while (!(*(USART2_SR) & USART_FLAG_RXNE));
	return *(USART2_DR) & 0xFF;
}

int fib(int number)
{
	if(number==0) return 0;
	int result;
	asm volatile("push {r3, r4, r5, r6}");
	asm volatile("mov r6, %[input]"::[input]"r"(number));
	asm volatile("mov r3, #-1\n" 
				 "mov r4, #1\n"
	 			 "mov r5, #0\n"
				 ".forloop:\n"
				 "add r5, r4, r3\n"
				 "mov r3, r4\n"
				 "mov r4, r5\n"
				 "subs r6, r6 ,#1\n"
				 "bge .forloop");
	asm volatile("mov %[output], r5"
				 :[output]"=r"(result)::);
	asm volatile("pop {r3, r4, r5, r6}");
	
	return result;
}

int strcmp(const char *str1, const char *str2)
{
	char c1, c2;
	while(*str1){
		c1 = *str1++;
		c2 = *str2++;
		if(c1!=c2) return 1;
	}
	return 0;
}

char *strtok(char *str, char *dot)
{
	static char *token;
	if(str!=NULL){
		token = str;
		while((*token)!=*dot){
			token++;
		}
		*token = '\0';
	}else{
		token++;
		if(*token == '\0') return NULL;
		str = token;
		while(((*token)!=*dot) || ((*token)!='\0')){
			if(*token == '\0') break;
			token++;
		}
		*token = '\0';
	}
	return str;
}
// calculate the length of string
int strlen(char *str)
{
	int i;
	for(i=0; str[i]!='\0'; i++);
	return i;
}
// reverse string
void reverse(char *str)
{
	int i, j;
	for(i=0, j=strlen(str)-1; i<j; i++, j--){
		char x = str[i];
		str[i] = str[j];
		str[j] = x;
	}
}
// string to integer
int atoi(char *str)
{
	int number = 0, i;
	for(i=0; str[i]!='\0'; i++){
		number = number*10 + str[i] - '0';
	}
	return number;
}
// integer to string
void itoa(int num, char str[])
{
	int check = 0; // check if number is negative or not
	if(num<0){
		num = -num;
		check=1;
	}
	int i=0;
	while(num!=0){
		str[i++] = num%10 + '0';
		num = num/10;
	}
	if(check) str[i++] = '-';
	str[i] = '\0';
	reverse(str);
	print_str(str);
	
}
void cmd_fib(void *number)
{
	while(1){
		while(mutex==UNLOCK); // when shell thread is locked, it can break while loop.
		char *num = (char *)number;
		int result = fib(atoi(num));
		itoa(result, num);
		mutex = UNLOCK;
	}
}
void cmd(char *command) 
{
	char *cm = strtok((char *)command, " ");
	if(!strcmp(cm, "fib")){
		cm = strtok(NULL, " ");
		if(thread_create(cmd_fib, (void *) cm) == -1)
			print_str("Thread cmd_fib creation failed\r\n");
		mutex = LOCK;
	}
	
}
void shell(void *userdata){
	char buf[30];
	int i;
	while(1){
		while(mutex==LOCK);
		print_str((char *) userdata);
		for(i=0;i<30;i++){
			buf[i] = get_char();
			// press Enter
			if(buf[i]==13){
				print_char("\n");
				buf[i]='\0';
				cmd(buf);
				break;
			}
			// backspace
			if((buf[i]==8) || (buf[i]==127)){
				print_str("\b \b");
				i--;
				continue;
			}
			print_char(&buf[i]);
		}
	}
}

/* 72MHz */
#define CPU_CLOCK_HZ 72000000

/* 100 ms per tick. */
#define TICK_RATE_HZ 10

int main(void)
{
	const char *str4 = "$ ";
	usart_init();

	if(thread_create(shell, (void *) str4) == -1)
		print_str("Thread 4 creation failed\r\n");

	/* SysTick configuration */

	*SYSTICK_LOAD = (CPU_CLOCK_HZ / TICK_RATE_HZ) - 1UL;
	*SYSTICK_VAL = 0;
	*SYSTICK_CTRL = 0x07;

	thread_start();

	return 0;
}
