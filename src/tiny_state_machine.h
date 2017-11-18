/*
 * tiny_state_machine.h
 */ 


#ifndef TINY_STATE_MACHINE_H_
#define TINY_STATE_MACHINE_H_

/* Define to include state names */
#define TINY_STATE_MACHINE_WITH_NAMES

/* Time & Space Efficient State machine handler */

/* Define the generic state element */
typedef struct {
    uint32_t _s;            /**< State Value */
#ifdef TINY_STATE_MACHINE_WITH_NAMES
    const char* _n;         /**< State Name */
#endif
    void (*_f)(void*);      /**< State Function */
} tiny_state_def;

/* Define the basic state machine context */
typedef struct {
    uint16_t        state;
    uint16_t        count;
    tiny_state_def* states;
} tiny_state_ctx;

/* The most stripped down state machine driver you can create */
static void inline tiny_state_init(void* context, tiny_state_def *states, uint16_t count, uint16_t initial)
{
    ((tiny_state_ctx*)context)->states = states;
    ((tiny_state_ctx*)context)->count = count;
    ((tiny_state_ctx*)context)->state = initial;
}

/* Search through a states list for the match */
static tiny_state_def * tiny_state_find(tiny_state_def *states, uint16_t count, uint16_t state)
{
    uint16_t i;
    for(i=0;i<count;i++)
    {
        if(states[i]._s == state)
        {
            return &states[i];
        }
    }
    return NULL;
}

/* Tiny state machine driver - doesn't require states to be ordered to be indexed */
static void inline tiny_state_driver(void* context)
{
    tiny_state_ctx * pCtx = (tiny_state_ctx*)context;
    tiny_state_def * pState = tiny_state_find(pCtx->states, pCtx->count, pCtx->state);
    if(pState && pState->_f)
    {
        pState->_f(context);
    }
}

/* Update the next state */
static void inline tiny_state_update(void* context, uint32_t next)
{
    ((tiny_state_ctx*)context)->state = next;
}

/* Retrieve the name of the state */
static const char* tiny_state_name(void* context, uint32_t state)
{
#ifdef TINY_STATE_MACHINE_WITH_NAMES
    tiny_state_ctx * pCtx = (tiny_state_ctx*)context;
    tiny_state_def * pState = tiny_state_find(pCtx->states, pCtx->count, state);

    return (pState && pState->_n)?pState->_n:"";
#else
    return "";
#endif
}

/* Helper macros */
#ifdef TINY_STATE_MACHINE_WITH_NAMES
#define TINY_STATE_DEF(x,y)     {x, #x, y}
#else
#define TINY_STATE_DEF(x,y)     {x, y}
#endif


#endif /* TINY_STATE_MACHINE_H_ */