#ifndef _NRF_DELAY_H
#define _NRF_DELAY_H

#include "nrf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for delaying execution for number of microseconds.
 *
 * @note NRF52 has instruction cache and because of that delay is not precise.
 *
 * @param number_of_us
 *
 */
/*lint --e{438, 522, 40, 10, 563} "Variable not used" "Function lacks side-effects" */
__STATIC_INLINE void nrf_delay_us(uint32_t number_of_us);


/**
 * @brief Function for delaying execution for number of miliseconds.
 *
 * @note NRF52 has instruction cache and because of that delay is not precise.
 *
 * @note Function internally calls @ref nrf_delay_us so the maximum delay is the
 * same as in case of @ref nrf_delay_us, approx. 71 minutes.
 *
 * @param number_of_ms
 *
 */

/*lint --e{438, 522, 40, 10, 563} "Variable not used" "Function lacks side-effects" */
__STATIC_INLINE void nrf_delay_ms(uint32_t number_of_ms);

#if defined ( __CC_ARM   )
__STATIC_INLINE void nrf_delay_us(uint32_t number_of_us)
{
    if(number_of_us)
    {
__asm
    {
loop:
    SUBS number_of_us,number_of_us, #1
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
#ifdef NRF52
     NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
#endif
    BNE    loop
    }
    }
}

#elif defined ( __ICCARM__ )

__STATIC_INLINE void nrf_delay_us(uint32_t number_of_us)
{
    if (number_of_us)
    {
__ASM volatile(
"1:\n\t"
       " SUBS %0, %0, #1\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
       " NOP\n\t"
#ifdef NRF52
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
        " NOP\n\t"
#endif
       " BNE.n 1b\n"
#if __CORTEX_M == (0x00U)
        :"=l" (number_of_us) : "0" (number_of_us)
#else
        :"=r" (number_of_us) : "0" (number_of_us)
#endif
        );
    }
}

#elif defined ( _WIN32 ) || defined ( __unix ) || defined( __APPLE__ )


#ifndef CUSTOM_NRF_DELAY_US
__STATIC_INLINE void nrf_delay_us(uint32_t number_of_us)
{}
#endif

#elif defined ( __GNUC__ )

__STATIC_INLINE void nrf_delay_us(uint32_t number_of_us)
{
    if (number_of_us)
    {
__ASM volatile (
#ifdef NRF51
        ".syntax unified\n"
#endif
    "1:\n"
    " SUBS %0, %0, #1\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
#ifdef NRF52
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
    " NOP\n"
#endif
    " BNE 1b\n"
#ifdef NRF51
    ".syntax divided\n"
#endif
#if __CORTEX_M == (0x00U)
    :"+l" (number_of_us));
#else
    :"+r" (number_of_us));
#endif
    }
}
#endif

__STATIC_INLINE void nrf_delay_ms(uint32_t number_of_ms)
{
    nrf_delay_us(1000*number_of_ms);
}


#ifdef __cplusplus
}
#endif

#endif
