#include "sidfx.h"

enum SIDFXState
{
	SIDFX_IDLE,
	SIDFX_RESET_0,
	SIDFX_RESET_1,
	SIDFX_READY,
	SIDFX_PLAY,
	SIDFX_WAIT
};

static struct SIDFXChannel
{
	const SIDFX	* volatile 		com;
	byte						delay, priority;
	volatile byte				cnt;
	volatile SIDFXState			state;
	unsigned					freq, pwm;

}	channels[3];

void sidfx_init(void)
{
	for(char i=0; i<3; i++)
	{
		channels[i].com = nullptr;
		channels[i].state = SIDFX_IDLE;
		channels[i].priority = 0;
	}
}

bool sidfx_idle(byte chn)
{
	return channels[chn].state == SIDFX_IDLE;
}

void sidfx_play(byte chn, const SIDFX * fx, byte cnt)
{
	SIDFXState		ns = channels[chn].state;

	if (ns == SIDFX_IDLE)
		ns = SIDFX_READY;
	else if (channels[chn].priority <= fx->priority)
		ns = SIDFX_RESET_0;
	else
		return;

	channels[chn].state = SIDFX_IDLE;

	channels[chn].com = fx;
	channels[chn].cnt = cnt - 1;
	channels[chn].priority = fx->priority;

	channels[chn].state = ns;
}

void sidfx_stop(byte chn)
{
	channels[chn].com = nullptr;
	if (channels[chn].state != SIDFX_IDLE)
		channels[chn].state = SIDFX_RESET_0;
}

inline void sidfx_loop_ch(byte ch)
{
	switch (channels[ch].state)
	{
		case SIDFX_IDLE:
			break;
		case SIDFX_RESET_0:
			sid.voices[ch].ctrl = 0;
			sid.voices[ch].attdec = 0;
			sid.voices[ch].susrel = 0;
			channels[ch].state = SIDFX_READY;
			break;
		case SIDFX_RESET_1:
//			sid.voices[ch].ctrl = SID_CTRL_TEST;
			channels[ch].state = SIDFX_READY;
			break;
		case SIDFX_READY:
			{
				const SIDFX	*	com = channels[ch].com;
				if (com)
				{
					channels[ch].freq = com->freq;
					channels[ch].pwm = com->pwm;

					sid.voices[ch].freq = com->freq;
					sid.voices[ch].pwm = com->pwm;
					sid.voices[ch].attdec = com->attdec;
					sid.voices[ch].susrel = com->susrel;
					sid.voices[ch].ctrl = com->ctrl;

					channels[ch].delay = com->time1;
					channels[ch].state = SIDFX_PLAY;
				}
				else
					channels[ch].state = SIDFX_IDLE;
			}
			break;
		case SIDFX_PLAY:
			{
				const SIDFX	*	com = channels[ch].com;
				if (com->dfreq)
				{
					channels[ch].freq += com->dfreq;
					sid.voices[ch].freq = channels[ch].freq;
				}
				if (com->dpwm)
				{
					channels[ch].pwm += com->dpwm;
					sid.voices[ch].pwm = channels[ch].pwm;
				}

				if (channels[ch].delay)
					channels[ch].delay--;
				else if (com->time0)
				{
					sid.voices[ch].ctrl = com->ctrl & ~SID_CTRL_GATE;
					channels[ch].delay = com->time0;
					channels[ch].state = SIDFX_WAIT;
				}
				else if (channels[ch].cnt)
				{
					com++;
					channels[ch].cnt--;
					channels[ch].com = com;
					channels[ch].priority = com->priority;
					channels[ch].state = SIDFX_READY;
				}
				else
				{
					channels[ch].com = nullptr;
					channels[ch].state = SIDFX_RESET_0;						
				}
			}
			break;
		case SIDFX_WAIT:
			{
				const SIDFX	*	com = channels[ch].com;			
				if (com->dfreq)
				{
					channels[ch].freq += com->dfreq;
					sid.voices[ch].freq = channels[ch].freq;
				}
				if (com->dpwm)
				{
					channels[ch].pwm += com->dpwm;
					sid.voices[ch].pwm = channels[ch].pwm;
				}

				if (channels[ch].delay)
					channels[ch].delay--;
				else if (channels[ch].cnt)
				{
					com++;
					channels[ch].cnt--;
					channels[ch].com = com;
					channels[ch].priority = com->priority;
					if (com->time0)
						channels[ch].state = SIDFX_RESET_0;
					else
						channels[ch].state = SIDFX_READY;
				}
				else
				{
					channels[ch].com = nullptr;
					channels[ch].state = SIDFX_RESET_0;					
				}
			}
			break;
	}
}

void sidfx_loop_2(void)
{
	sidfx_loop_ch(2);	
}

void sidfx_loop(void)
{
	for(byte ch=0; ch<3; ch++)
		sidfx_loop_ch(ch);
}	
