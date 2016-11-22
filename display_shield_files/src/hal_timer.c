/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#include "hal_timer.h"

#include "nrf.h"


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


#define ACCESS_STATUS_OPENED_BIT(id)             (0x1  << ((id) * 8))
#define ACCESS_STATUS_SHARED_BIT(id)             (0x1  << ((id) * 8 + 1))
#define ACCESS_STATUS_OPENED_AND_SHARED_BITS(id) (0x3  << ((id) * 8))
#define ACCESS_STATUS_CC_ACQUIRED_BIT(id, cc_id) (0x1  << ((id) * 8 + 2 + (cc_id)))
#define ACCESS_STATUS_ALL_CC_ACQUIRED_BITS(id)   (0xF  << ((id) * 8 + 2))
#define ACCESS_STATUS_ALL_USED_BITS(id)          (0x3F << ((id) * 8))


typedef union
{
    struct
    {
        uint32_t    timer0_opened       :  1;
        uint32_t    timer0_shared       :  1;
        uint32_t    timer0_cc0_acquired :  1;
        uint32_t    timer0_cc1_acquired :  1;
        uint32_t    timer0_cc2_acquired :  1;
        uint32_t    timer0_cc3_acquired :  1;
        uint32_t    rfu0                :  2;
        uint32_t    timer1_opened       :  1;
        uint32_t    timer1_shared       :  1;
        uint32_t    timer1_cc0_acquired :  1;
        uint32_t    timer1_cc1_acquired :  1;
        uint32_t    timer1_cc2_acquired :  1;
        uint32_t    timer1_cc3_acquired :  1;
        uint32_t    rfu1                :  2;
        uint32_t    timer2_opened       :  1;
        uint32_t    timer2_shared       :  1;
        uint32_t    timer2_cc0_acquired :  1;
        uint32_t    timer2_cc1_acquired :  1;
        uint32_t    timer2_cc2_acquired :  1;
        uint32_t    timer2_cc3_acquired :  1;
        uint32_t    rfu                 : 10;

    } fields;
    uint32_t    bits;
} m_access_status_t;


typedef struct
{
    hal_timer_sig_callback_t    signal_handlers[4];
    hal_timer_cc_mode_t         cc_modes[4];
    uint8_t                     user_count;
} m_hal_timer_t;


struct
{
    m_hal_timer_t   timers[3];
    
    m_access_status_t access_status;
} m_hal_timer;


static void context_get(hal_timer_id_t id, NRF_TIMER_Type **timer, m_hal_timer_t **context)
{
    switch ( id )
    {
#ifdef HAL_TIMER_TIMER0
        case HAL_TIMER_ID_TIMER0:
            *context = &(m_hal_timer.timers[id]);
            *timer = NRF_TIMER0;
            break;
#endif
#ifdef HAL_TIMER_TIMER1
        case HAL_TIMER_ID_TIMER1:
            *context = &(m_hal_timer.timers[id]);
            *timer = NRF_TIMER1;
            break;
#endif
#ifdef HAL_TIMER_TIMER2
        case HAL_TIMER_ID_TIMER2:
            *context = &(m_hal_timer.timers[id]);
            *timer = NRF_TIMER2;
            break;
#endif
        default:
            break;
    }
}


//static bool peripheral_is_available(hal_timer_id_t id, hal_timer_access_mode_t access_mode)
//{
//    if ( (ACCESS_STATUS_OPENED_BIT(id) & m_hal_timer.access_status.bits) == 0 )
//    {
//        return ( true );
//    }
//    else if ( (access_mode                                                     == HAL_TIMER_ACCESS_MODE_SHARED) 
//    &&        ((ACCESS_STATUS_SHARED_BIT(id) & m_hal_timer.access_status.bits) != 0) )
//    {
//        return ( true );
//        
//    }
//    
//    return ( false );
//}


static uint32_t peripheral_acquire(hal_timer_id_t id, hal_timer_access_mode_t access_mode)
{
    if ( (ACCESS_STATUS_OPENED_BIT(id) & m_hal_timer.access_status.bits) == 0 )
    {
        if ( access_mode == HAL_TIMER_ACCESS_MODE_SHARED ) 
        {
            m_hal_timer.access_status.bits |= ACCESS_STATUS_OPENED_AND_SHARED_BITS(id);
        }
        else
        {
            m_hal_timer.access_status.bits |= ACCESS_STATUS_OPENED_BIT(id);
        }
        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    else if ( (access_mode                                                     == HAL_TIMER_ACCESS_MODE_SHARED) 
    &&        ((ACCESS_STATUS_SHARED_BIT(id) & m_hal_timer.access_status.bits) != 0) )
    {
        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
}


static uint32_t peripheral_release(hal_timer_id_t id)
{
    NRF_TIMER_Type *timer;
    m_hal_timer_t *context;
    
    if ( (ACCESS_STATUS_OPENED_BIT(id) & m_hal_timer.access_status.bits) != 0 )
    {

        context_get(id, &timer, &context);
        
        if ( context->user_count > 0 )
        {
            --context->user_count;
        
            if ( context->user_count == 0 )
            {
#ifdef NRF51
                timer->POWER = (TIMER_POWER_POWER_Disabled << TIMER_POWER_POWER_Pos);
#endif
#ifdef NRF52
                timer->TASKS_STOP = 1;
                timer->TASKS_CLEAR = 1;
#endif
                
                context->cc_modes[0] = HAL_TIMER_CC_MODE_DEFAULT;
                context->cc_modes[1] = HAL_TIMER_CC_MODE_DEFAULT;
                context->cc_modes[2] = HAL_TIMER_CC_MODE_DEFAULT;
                context->cc_modes[3] = HAL_TIMER_CC_MODE_DEFAULT;
            
                m_hal_timer.access_status.bits &= ~ACCESS_STATUS_ALL_USED_BITS(id);
            }
        }

        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
}


static bool cc_instance_is_acquired(hal_timer_id_t id, hal_timer_cc_id_t cc_id)
{
    if ( ((ACCESS_STATUS_OPENED_BIT(id)             & m_hal_timer.access_status.bits) != 0)
    &&   ((ACCESS_STATUS_CC_ACQUIRED_BIT(id, cc_id) & m_hal_timer.access_status.bits) != 0) )
    {
        return ( true );
    }
    
    return ( false );
}


static uint32_t cc_instance_acquire(hal_timer_id_t id, hal_timer_cc_id_t cc_id)
{
    if ( ((ACCESS_STATUS_OPENED_BIT(id)             & m_hal_timer.access_status.bits) != 0)
    &&   ((ACCESS_STATUS_CC_ACQUIRED_BIT(id, cc_id) & m_hal_timer.access_status.bits) == 0) )
    {
        m_hal_timer.access_status.bits |= ACCESS_STATUS_CC_ACQUIRED_BIT(id, cc_id);
        
        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
}


static uint32_t cc_instance_release(hal_timer_id_t id, hal_timer_cc_id_t cc_id)
{
    NRF_TIMER_Type *timer;
    m_hal_timer_t *context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0
            case HAL_TIMER_CC_ID_0:
                timer->SHORTS &= ~( (TIMER_SHORTS_COMPARE0_STOP_Enabled  << TIMER_SHORTS_COMPARE0_STOP_Pos)
                               |    (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos) );
                timer->INTENCLR = (TIMER_INTENCLR_COMPARE0_Clear << TIMER_INTENCLR_COMPARE0_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                timer->SHORTS &= ~( (TIMER_SHORTS_COMPARE1_STOP_Enabled  << TIMER_SHORTS_COMPARE1_STOP_Pos)
                               |    (TIMER_SHORTS_COMPARE1_CLEAR_Enabled << TIMER_SHORTS_COMPARE1_CLEAR_Pos) );
                timer->INTENCLR = (TIMER_INTENCLR_COMPARE1_Clear << TIMER_INTENCLR_COMPARE1_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                timer->SHORTS &= ~( (TIMER_SHORTS_COMPARE2_STOP_Enabled  << TIMER_SHORTS_COMPARE2_STOP_Pos)
                               |    (TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos) );
                timer->INTENCLR = (TIMER_INTENCLR_COMPARE2_Clear << TIMER_INTENCLR_COMPARE2_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                timer->SHORTS &= ~( (TIMER_SHORTS_COMPARE3_STOP_Enabled  << TIMER_SHORTS_COMPARE3_STOP_Pos)
                               |    (TIMER_SHORTS_COMPARE3_CLEAR_Enabled << TIMER_SHORTS_COMPARE3_CLEAR_Pos) );
                timer->INTENCLR = (TIMER_INTENCLR_COMPARE3_Clear << TIMER_INTENCLR_COMPARE3_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        m_hal_timer.access_status.bits &= ~ACCESS_STATUS_CC_ACQUIRED_BIT(id, cc_id);
        
        /* Check it it is a shared peripheral and if this is the last cc instance
           to be released before starting the timer. */
        if ( ((m_hal_timer.access_status.bits & ACCESS_STATUS_SHARED_BIT(id))     != 0)
        &&   ((m_hal_timer.access_status.bits & ACCESS_STATUS_ALL_CC_ACQUIRED_BITS(id)) == 0) )
        {
            timer->TASKS_STOP = 1;
        }

        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
}


static bool hw_config_is_compatible(hal_timer_id_t id, hal_timer_cfg_t const * const cfg)
{
    NRF_TIMER_Type *timer = NULL;
    m_hal_timer_t *context;
    
    context_get(id, &timer, &context);
    
    if ( (timer != NULL)
    &&     ((context->user_count == 0)
      ||    
            ((timer->MODE      == cfg->mode     )   
        &&   (timer->PRESCALER == cfg->prescaler)
        &&   (timer->BITMODE   == cfg->bitmode  ))) )
    {
        return ( true );
    }
    
    return ( false );
}


void m_hal_timer_isr_handler(NRF_TIMER_Type *timer, m_hal_timer_t *context, hal_timer_cc_id_t cc_id)
{
    bool done = ( context->cc_modes[cc_id] == HAL_TIMER_CC_MODE_CALLBACK );
    
    do
    {
        if ( timer->EVENTS_COMPARE[cc_id] != 0 )
        {
            timer->EVENTS_COMPARE[cc_id] = 0;
            
            if ( context->cc_modes[cc_id] == HAL_TIMER_CC_MODE_CALLBACK )
            {
                context->signal_handlers[cc_id]((hal_timer_signal_type_t)cc_id);
            }
            done = true;
        }
    }
    while ( !done );
}


static void m_hal_timer_isr_dispatch(hal_timer_id_t id)
{
    NRF_TIMER_Type *timer;
    m_hal_timer_t *context;
    
    context_get(id, &timer, &context);

#ifdef HAL_TIMER_CC0    
    if ( (timer->EVENTS_COMPARE[0] != 0)
    &&   (timer->INTENSET & (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos)) )
    {
        m_hal_timer_isr_handler(timer, context, HAL_TIMER_CC_ID_0);
    }
#endif
#ifdef HAL_TIMER_CC1
    if ( (timer->EVENTS_COMPARE[1] != 0)
    &&   (timer->INTENSET & (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos)) )
    {
        m_hal_timer_isr_handler(timer, context, HAL_TIMER_CC_ID_1);
    }
#endif
#ifdef HAL_TIMER_CC2
    if ( (timer->EVENTS_COMPARE[2] != 0)
    &&   (timer->INTENSET & (TIMER_INTENSET_COMPARE2_Enabled << TIMER_INTENSET_COMPARE2_Pos)) )
    {
        m_hal_timer_isr_handler(timer, context, HAL_TIMER_CC_ID_2);
    }
#endif
#ifdef HAL_TIMER_CC3
    if ( (timer->EVENTS_COMPARE[3] != 0)
    &&   (timer->INTENSET & (TIMER_INTENSET_COMPARE3_Enabled << TIMER_INTENSET_COMPARE3_Pos)) )
    {
        m_hal_timer_isr_handler(timer, context, HAL_TIMER_CC_ID_3);
    }
#endif
}


void hal_timer_init(void)
{
    m_hal_timer.access_status.bits = 0;
    m_hal_timer.timers[0].user_count = 0;
    m_hal_timer.timers[1].user_count = 0;
    m_hal_timer.timers[2].user_count = 0;
    
#ifdef HAL_TIMER_TIMER0
    NRF_TIMER0->INTENCLR = 0xFFFFFFFF;
	NVIC_SetPriority(TIMER0_IRQn, 7);
    NVIC_EnableIRQ(TIMER0_IRQn);
#endif
#ifdef HAL_TIMER_TIMER1
    NRF_TIMER1->INTENCLR = 0xFFFFFFFF;
	NVIC_SetPriority(TIMER1_IRQn, 7);
    NVIC_EnableIRQ(TIMER1_IRQn);
#endif
#ifdef HAL_TIMER_TIMER2
    NRF_TIMER2->INTENCLR = 0xFFFFFFFF;
	NVIC_SetPriority(TIMER2_IRQn, 7);
    NVIC_EnableIRQ(TIMER2_IRQn);
#endif
}


uint32_t hal_timer_open(hal_timer_id_t *p_id, hal_timer_cfg_t const * const p_cfg)
{
    NRF_TIMER_Type *timer;
    m_hal_timer_t *context;

    if ( *p_id != HAL_TIMER_ID_NONE )
    {
        if ( ((p_cfg->access_mode == HAL_TIMER_ACCESS_MODE_SHARED) && (!hw_config_is_compatible(*p_id, p_cfg)))
        ||   (peripheral_acquire(*p_id, p_cfg->access_mode) != HAL_TIMER_STATUS_CODE_SUCCESS) )
        {
            return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
    }
    else
    {
        if ( false )
        {
        } 
#ifdef HAL_TIMER_TIMER0
        else if ( ((p_cfg->access_mode != HAL_TIMER_ACCESS_MODE_SHARED) || hw_config_is_compatible(HAL_TIMER_ID_TIMER0, p_cfg))
        &&   (peripheral_acquire(HAL_TIMER_ID_TIMER0, p_cfg->access_mode) == HAL_TIMER_STATUS_CODE_SUCCESS) )
        {
            *p_id = HAL_TIMER_ID_TIMER0;
        }
#endif
#ifdef HAL_TIMER_TIMER1
        else if ( ((p_cfg->access_mode != HAL_TIMER_ACCESS_MODE_SHARED) || hw_config_is_compatible(HAL_TIMER_ID_TIMER1, p_cfg))
        &&   (peripheral_acquire(HAL_TIMER_ID_TIMER1, p_cfg->access_mode) == HAL_TIMER_STATUS_CODE_SUCCESS) )
        {
            *p_id = HAL_TIMER_ID_TIMER1;
        }
#endif
#ifdef HAL_TIMER_TIMER2
        else if ( ((p_cfg->access_mode != HAL_TIMER_ACCESS_MODE_SHARED) || hw_config_is_compatible(HAL_TIMER_ID_TIMER2, p_cfg))
        &&   (peripheral_acquire(HAL_TIMER_ID_TIMER2, p_cfg->access_mode) == HAL_TIMER_STATUS_CODE_SUCCESS) )
        {
            *p_id = HAL_TIMER_ID_TIMER2;
        }
#endif
        else
        {
            return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
    }
    
    context_get(*p_id, &timer, &context);
    
    if ( context->user_count == 0 )
    {
#ifdef NRF51
        timer->POWER = (TIMER_POWER_POWER_Disabled << TIMER_POWER_POWER_Pos);
        timer->POWER = (TIMER_POWER_POWER_Enabled  << TIMER_POWER_POWER_Pos);
#endif
#ifdef NRF52
        timer->TASKS_STOP = 1;
        timer->TASKS_CLEAR = 1;
#endif
    
        timer->BITMODE      = p_cfg->bitmode;
        timer->MODE         = p_cfg->mode;
        timer->PRESCALER    = p_cfg->prescaler;
    }
    ++context->user_count;
    
    return ( HAL_TIMER_STATUS_CODE_SUCCESS );    
}


uint32_t hal_timer_cc_acquire(hal_timer_id_t id, hal_timer_cc_id_t *p_cc_id, uint32_t ** p_evt_addr)
{
    uint32_t * p_tmp_evt_addr       = NULL;
    m_access_status_t access_status = m_hal_timer.access_status;     
    NRF_TIMER_Type *timer;
    m_hal_timer_t *context;    
    
    if ( *p_cc_id != HAL_TIMER_CC_ID_NONE )
    {
        if ( cc_instance_acquire(id, *p_cc_id) != HAL_TIMER_STATUS_CODE_SUCCESS )
        {
            return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
    }
    else
    {
        context_get(id, &timer, &context);

        if ( false )
        {
        }
#ifdef HAL_TIMER_CC0
        else if ( cc_instance_acquire(id, HAL_TIMER_CC_ID_0) == HAL_TIMER_STATUS_CODE_SUCCESS )
        {
            *p_cc_id       = HAL_TIMER_CC_ID_0;
            p_tmp_evt_addr = (uint32_t *)&(timer->EVENTS_COMPARE[0]);
        }
#endif
#ifdef HAL_TIMER_CC1
        else if ( cc_instance_acquire(id, HAL_TIMER_CC_ID_1) == HAL_TIMER_STATUS_CODE_SUCCESS )
        {
            *p_cc_id       = HAL_TIMER_CC_ID_1;
            p_tmp_evt_addr = (uint32_t *)&(timer->EVENTS_COMPARE[1]);
        }
#endif
#ifdef HAL_TIMER_CC2
        else if ( cc_instance_acquire(id, HAL_TIMER_CC_ID_2) == HAL_TIMER_STATUS_CODE_SUCCESS )
        {
            *p_cc_id       = HAL_TIMER_CC_ID_2;
            p_tmp_evt_addr = (uint32_t *)&(timer->EVENTS_COMPARE[2]);
        }
#endif
#ifdef HAL_TIMER_CC3
        else if ( cc_instance_acquire(id, HAL_TIMER_CC_ID_3) == HAL_TIMER_STATUS_CODE_SUCCESS )
        {
            *p_cc_id       = HAL_TIMER_CC_ID_3;
            p_tmp_evt_addr = (uint32_t *)&(timer->EVENTS_COMPARE[3]);
        }
#endif
        else
        {
            return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
    }
    
    context->cc_modes[*p_cc_id] = HAL_TIMER_CC_MODE_DEFAULT;
    
    *p_evt_addr = p_tmp_evt_addr;

    /* Check it it is a shared peripheral and if this is the first cc instance
       to be acquired before starting the timer. */
    if ( ((m_hal_timer.access_status.bits & ACCESS_STATUS_SHARED_BIT(id))           != 0)
    &&   ((            access_status.bits & ACCESS_STATUS_ALL_CC_ACQUIRED_BITS(id)) == 0) )
    {
        timer->TASKS_START = 1;
    }
    
    return ( HAL_TIMER_STATUS_CODE_SUCCESS );
}


uint32_t hal_timer_shorts_set(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t set_mask)
{
    uint32_t mask;
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        (void)context;
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0            
            case HAL_TIMER_CC_ID_0:
                mask = (TIMER_SHORTS_COMPARE0_STOP_Enabled  << TIMER_SHORTS_COMPARE0_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                mask = (TIMER_SHORTS_COMPARE1_STOP_Enabled  << TIMER_SHORTS_COMPARE1_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE1_CLEAR_Enabled << TIMER_SHORTS_COMPARE1_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                mask = (TIMER_SHORTS_COMPARE2_STOP_Enabled  << TIMER_SHORTS_COMPARE2_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                mask = (TIMER_SHORTS_COMPARE3_STOP_Enabled  << TIMER_SHORTS_COMPARE3_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE3_CLEAR_Enabled << TIMER_SHORTS_COMPARE3_CLEAR_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        if ( (mask | set_mask) == mask )
        {
            timer->SHORTS |= set_mask;
            return ( HAL_TIMER_STATUS_CODE_SUCCESS );
        }
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED );  
}


uint32_t hal_timer_shorts_clear(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t clear_mask)
{
    uint32_t mask;
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        (void)context;
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0
            case HAL_TIMER_CC_ID_0:
                mask = (TIMER_SHORTS_COMPARE0_STOP_Enabled  << TIMER_SHORTS_COMPARE0_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                mask = (TIMER_SHORTS_COMPARE1_STOP_Enabled  << TIMER_SHORTS_COMPARE1_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE1_CLEAR_Enabled << TIMER_SHORTS_COMPARE1_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                mask = (TIMER_SHORTS_COMPARE2_STOP_Enabled  << TIMER_SHORTS_COMPARE2_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                mask = (TIMER_SHORTS_COMPARE3_STOP_Enabled  << TIMER_SHORTS_COMPARE3_STOP_Pos)
                     | (TIMER_SHORTS_COMPARE3_CLEAR_Enabled << TIMER_SHORTS_COMPARE3_CLEAR_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        if ( (mask | clear_mask) == mask )
        {
            timer->SHORTS &= ~clear_mask;
            return ( HAL_TIMER_STATUS_CODE_SUCCESS );
        }
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED ); 
}


uint32_t hal_timer_cc_reg_set(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t value)
{
    uint32_t mask;
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0
            case HAL_TIMER_CC_ID_0:
                mask = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
                break;                
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                mask = (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                mask = (TIMER_INTENSET_COMPARE2_Enabled << TIMER_INTENSET_COMPARE2_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                mask = (TIMER_INTENSET_COMPARE3_Enabled << TIMER_INTENSET_COMPARE3_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        
        timer->CC[cc_id] = value;
        timer->EVENTS_COMPARE[cc_id] = 0;
        if ( context->cc_modes[cc_id] == HAL_TIMER_CC_MODE_CALLBACK )
        {
            timer->INTENSET = mask;
        }
        else if ( context->cc_modes[cc_id] == HAL_TIMER_CC_MODE_POLLING )
        {
            m_hal_timer_isr_handler(timer, context, cc_id);
        }
        
        return ( HAL_TIMER_STATUS_CODE_SUCCESS ); 
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED ); 
}

uint32_t hal_timer_cc_reg_get(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t * p_value)
{
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        
        *p_value = timer->CC[cc_id];
        
        return ( HAL_TIMER_STATUS_CODE_SUCCESS ); 
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED ); 
}


uint32_t hal_timer_cc_reg_clear(hal_timer_id_t id, hal_timer_cc_id_t cc_id)
{
    uint32_t mask;
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0
            case HAL_TIMER_CC_ID_0:
                mask = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
                break;                
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                mask = (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                mask = (TIMER_INTENSET_COMPARE2_Enabled << TIMER_INTENSET_COMPARE2_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                mask = (TIMER_INTENSET_COMPARE3_Enabled << TIMER_INTENSET_COMPARE3_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        
        timer->INTENCLR = mask;
        timer->CC[cc_id] = 0;
        
        return ( HAL_TIMER_STATUS_CODE_SUCCESS ); 
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED ); 
}


uint32_t * hal_timer_reg_addr_get(hal_timer_id_t id, hal_timer_cc_id_t cc_id, hal_timer_hw_reg_id_t reg_id)
{
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    uint32_t *          reg_addr = NULL;
    
    if ( (ACCESS_STATUS_OPENED_BIT(id) & m_hal_timer.access_status.bits) != 0 )
    {
        context_get(id, &timer, &context);
        
        if ( (ACCESS_STATUS_SHARED_BIT(id) & m_hal_timer.access_status.bits) == 0 )
        {
            switch ( reg_id )
            {
                case HAL_TIMER_HW_REG_ID_TASKS_START:
                    reg_addr = (uint32_t *)&(timer->TASKS_START);
                    break;
                case HAL_TIMER_HW_REG_ID_TASKS_STOP:
                    reg_addr = (uint32_t *)&(timer->TASKS_STOP);
                    break;
                case HAL_TIMER_HW_REG_ID_TASKS_COUNT:
                    reg_addr = (uint32_t *)&(timer->TASKS_COUNT);
                    break;
                case HAL_TIMER_HW_REG_ID_TASKS_CLEAR:
                    reg_addr = (uint32_t *)&(timer->TASKS_CLEAR);
                    break;
                default:
                    break;
            }
        }
            
        if ( (reg_addr == NULL)
        &&   (cc_instance_is_acquired(id, cc_id)) )
        {
            switch ( reg_id )
            {
#ifdef HAL_TIMER_CC0
                case HAL_TIMER_HW_REG_ID_TASKS_CAPTURE0:
                    if ( cc_id == HAL_TIMER_CC_ID_0 )
                    {
                        reg_addr = (uint32_t *)&(timer->TASKS_CAPTURE[0]);
                    }
                    break;
                case HAL_TIMER_HW_REG_ID_EVENTS_COMPARE0:
                    if ( cc_id == HAL_TIMER_CC_ID_0 )
                    {
                        reg_addr = (uint32_t *)&(timer->EVENTS_COMPARE[0]);
                    }
                    break;
#endif
#ifdef HAL_TIMER_CC1
                case HAL_TIMER_HW_REG_ID_TASKS_CAPTURE1:
                    if ( cc_id == HAL_TIMER_CC_ID_1 )
                    {
                        reg_addr = (uint32_t *)&(timer->TASKS_CAPTURE[1]);
                    }
                    break;
                case HAL_TIMER_HW_REG_ID_EVENTS_COMPARE1:
                    if ( cc_id == HAL_TIMER_CC_ID_1)
                    {
                        reg_addr = (uint32_t *)&(timer->EVENTS_COMPARE[1]);
                    }
                    break;
#endif
#ifdef HAL_TIMER_CC2
                case HAL_TIMER_HW_REG_ID_TASKS_CAPTURE2:
                    if ( cc_id == HAL_TIMER_CC_ID_2 )
                    {
                        reg_addr = (uint32_t *)&(timer->TASKS_CAPTURE[2]);
                    }
                    break;
                case HAL_TIMER_HW_REG_ID_EVENTS_COMPARE2:
                    if ( cc_id == HAL_TIMER_CC_ID_2 )
                    {
                        reg_addr = (uint32_t *)&(timer->EVENTS_COMPARE[2]);
                    }
                    break;
#endif
#ifdef HAL_TIMER_CC3
                case HAL_TIMER_HW_REG_ID_TASKS_CAPTURE3:
                    if ( cc_id == HAL_TIMER_CC_ID_3 )
                    {
                        reg_addr = (uint32_t *)&(timer->TASKS_CAPTURE[3]);
                    }
                    break;
                case HAL_TIMER_HW_REG_ID_EVENTS_COMPARE3:
                    if ( cc_id == HAL_TIMER_CC_ID_3 )
                    {
                        reg_addr = (uint32_t *)&(timer->EVENTS_COMPARE[3]);
                    }
                    break;
#endif
                default:
                    break;
            }
        }
    }
    return reg_addr;
}


uint32_t hal_timer_cc_release(hal_timer_id_t id, hal_timer_cc_id_t cc_id)
{
    return ( cc_instance_release(id, cc_id) );
}


uint32_t hal_timer_cc_configure(hal_timer_id_t id, hal_timer_cc_id_t cc_id, hal_timer_cc_mode_t mode, hal_timer_sig_callback_t hal_timer_sig_callback)
{
    uint32_t            mask;
    NRF_TIMER_Type *    timer;
    m_hal_timer_t *     context;
    
    if ( cc_instance_is_acquired(id, cc_id) )
    {
        context_get(id, &timer, &context);
        
        switch ( cc_id )
        {
#ifdef HAL_TIMER_CC0
            case HAL_TIMER_CC_ID_0:
                mask = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
                break;                
#endif
#ifdef HAL_TIMER_CC1
            case HAL_TIMER_CC_ID_1:
                mask = (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC2
            case HAL_TIMER_CC_ID_2:
                mask = (TIMER_INTENSET_COMPARE2_Enabled << TIMER_INTENSET_COMPARE2_Pos);
                break;
#endif
#ifdef HAL_TIMER_CC3
            case HAL_TIMER_CC_ID_3:
                mask = (TIMER_INTENSET_COMPARE3_Enabled << TIMER_INTENSET_COMPARE3_Pos);
                break;
#endif
            default:
                return ( HAL_TIMER_STATUS_CODE_DISALLOWED );
        }
        
        if ( mode == HAL_TIMER_CC_MODE_CALLBACK )
        {
            if ( hal_timer_sig_callback != NULL )
            {                
                context->cc_modes[cc_id]        = mode;
                context->signal_handlers[cc_id] = hal_timer_sig_callback;

                timer->INTENSET = mask;
            }
            else
            {
                return ( HAL_TIMER_STATUS_CODE_INVALID_PARAM );
            }
        }
        else
        {
            timer->INTENCLR = mask;

            context->cc_modes[cc_id]        = mode;            
            context->signal_handlers[cc_id] = NULL;
        }
        
        return ( HAL_TIMER_STATUS_CODE_SUCCESS );
    }
    
    return ( HAL_TIMER_STATUS_CODE_DISALLOWED ); 
}


uint32_t hal_timer_close(hal_timer_id_t id)
{
    return ( peripheral_release(id) );
}

#ifdef HAL_TIMER_TIMER0
void TIMER0_IRQHandler(void)
{
    m_hal_timer_isr_dispatch(HAL_TIMER_ID_TIMER0);
}
#endif


#ifdef HAL_TIMER_TIMER1
void TIMER1_IRQHandler(void)
{
    m_hal_timer_isr_dispatch(HAL_TIMER_ID_TIMER1);
}
#endif

#ifdef HAL_TIMER_TIMER2
void TIMER2_IRQHandler(void)
{
    m_hal_timer_isr_dispatch(HAL_TIMER_ID_TIMER2);
}
#endif
