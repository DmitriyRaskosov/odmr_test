import spinapi as pb
print("spinapi loaded from:", pb.__file__)
from dataclasses import dataclass
import time


CH0 =0x1
CH1 = 0x1 << 1
CH2 = 0x1 << 2
CH3 = 0x1 << 3
CH4 = 0x1 << 4
CH5 = 0x1 << 5
CH6 = 0x1 << 6
CH7 = 0x1 << 7
CH8 = 0x1 << 8
CH9 = 0x1 << 9
CH10 = 0x1 << 10
CH11 = 0x1 << 11
CH12 = 0x1 << 12
CH13 = 0x1 << 13
CH14 = 0x1 << 14
CH15 = 0x1 << 15
CH16 = 0x1 << 16
CH17 = 0x1 << 17
CH18 = 0x1 << 18
CH19 = 0x1 << 19
CH20 = 0x1 << 20
CH21 = 0x1 << 21
CH22 = 0x1 << 22
CH23 = 0x1 << 223

def spincore_init():
    res = pb.pb_init()
    if res != 0:
        print("Init failed:", pb.pb_status_message(), res, pb.pb_get_error())
        exit()
    pb.pb_core_clock(500.0)
    pb.pb_set_defaults()

#CH0 LASER
#CH1 СВЧ
#CH2 ИЗМЕРЕНИЕ
#CH3 SWEEP
def build_impulses_for_cv_odmr(num_meas, t1,t2,t4,t5,num_avg):
    spincore_init()
    pb.pb_reset()
    pb.pb_start_programming(pb.PULSE_PROGRAM)
    
    #outer
    begin = pb.pb_inst_pbonly(0, pb.LOOP, num_meas ,500)

    #inner
    inner = pb.pb_inst_pbonly(pb.ON |CH0|CH2, pb.LOOP, num_avg ,t1)
    
    pb.pb_inst_pbonly(pb.ON | CH0, pb.CONTINUE, 0,t2)
    pb.pb_inst_pbonly(pb.ON | CH0|CH1|CH2, pb.CONTINUE, 0,t4)
    
    pb.pb_inst_pbonly(0, pb.END_LOOP, inner, 1*pb.us)#end inner 
    
    pb.pb_inst_pbonly(0, pb.CONTINUE, 0, t5)#delay
    pb.pb_inst_pbonly(pb.ON | CH3, pb.CONTINUE, 0, 10*pb.us) #swp    
   

    pb.pb_inst_pbonly(0, pb.END_LOOP, begin, 1*pb.ms) #end outer
    pb.pb_inst_pbonly(0x00, pb.STOP, 0, 100*pb.ns)
    pb.pb_stop_programming()
    pb.pb_start()
    pb.pb_close()
    return 0
def build_impulses_rabi(t1,t2_start,t2_end,t2_step,t3,t4,t5,t6,t7,num_meas,delay_between_measurements):
    spincore_init()
    pb.pb_reset()
    pb.pb_start_programming(pb.PULSE_PROGRAM)
    num_points_max = 400
    num_points = int(round((t2_end - t2_start) / t2_step)) + 1
    if num_points_max < num_points:
        print(f"Too many points:{num_points}, max number is {num_points_max}")
        exit(1)
    #outer
    #begin=pb.pb_inst_pbonly(0x00, pb.CONTINUE, 0, 5 * pb.ms)  
    for i in range(num_points):
        loop = pb.pb_inst_pbonly(0, pb.LOOP, num_meas, t1)
        pb.pb_inst_pbonly(pb.ON | CH1, pb.CONTINUE, 0, t2_start + i*t2_step)
        pb.pb_inst_pbonly(0,pb.CONTINUE,0,t3)
        pb.pb_inst_pbonly(pb.ON|CH0,pb.CONTINUE,0,t4)
        pb.pb_inst_pbonly(pb.ON|CH0|CH2, pb.CONTINUE, 0, t5)
        pb.pb_inst_pbonly(pb.ON|CH0,pb.CONTINUE,0,t6)
        pb.pb_inst_pbonly(pb.ON|CH0|CH2, pb.CONTINUE, 0, t7)
        pb.pb_inst_pbonly(0, pb.END_LOOP, loop, delay_between_measurements)
        

    #pb.pb_inst_pbonly(0x00, pb.BRANCH, start, 50 * pb.ns)  # все нули, задержка 5 мс между измерениями
    
    pb.pb_stop_programming()
    pb.pb_start()
    pb.pb_close()
    return 0
def build_impulses_for_impulse_odmr(t1,t2_start,t2_end,t2_step,t3,t4,t5,t6,t7,t8,num_meas,delay_between_measurements):
    spincore_init()
    pb.pb_reset()
    pb.pb_start_programming(pb.PULSE_PROGRAM)
    num_points_max = 400
    num_points = int(round((t2_end - t2_start) / t2_step)) + 1
    if num_points_max < num_points:
        print(f"Too many points:{num_points}, max number is {num_points_max}")
        exit(1)
    #outer
    #begin=pb.pb_inst_pbonly(0x00, pb.CONTINUE, 0, 5 * pb.ms)  
    for i in range(num_points):
        loop = pb.pb_inst_pbonly(0, pb.LOOP, num_meas, t1)
        pb.pb_inst_pbonly(pb.ON | CH1, pb.CONTINUE, 0, t2_start + i*t2_step)
        pb.pb_inst_pbonly(0,pb.CONTINUE,0,t3)
        pb.pb_inst_pbonly(pb.ON|CH0,pb.CONTINUE,0,t4)
        pb.pb_inst_pbonly(pb.ON|CH0|CH2, pb.CONTINUE, 0, t5)
        pb.pb_inst_pbonly(pb.ON|CH0,pb.CONTINUE,0,t6)
        pb.pb_inst_pbonly(pb.ON|CH0|CH2, pb.CONTINUE, 0, t7)
        pb.pb_inst_pbonly(0, pb.END_LOOP, loop, delay_between_measurements)

        pb.pb_inst_pbonly(0, pb.CONTINUE, 0, t8)#delay
        pb.pb_inst_pbonly(pb.ON | CH3, pb.CONTINUE, 0, 10*pb.us) #swp  

    #pb.pb_inst_pbonly(0x00, pb.BRANCH, start, 50 * pb.ns)  # все нули, задержка 5 мс между измерениями
    
    pb.pb_stop_programming()
    pb.pb_start()
    pb.pb_close()
    return 0
if __name__ == "__main__":
    ms = pb.ms
    ns=pb.ns
    us=pb.us
    build_impulses_for_cv_odmr(num_meas = 500,t1=50*pb.us, t2=500*pb.us, t4=10*pb.us,t5=50*pb.us, num_avg=100)
    #build_impulses_rabi(t1=20*us, t2_start=5*us, t2_end=20*us, t2_step=1*us, t3=20*us,t4=10*us,t5=20*us,t6=10*us,t7=12*us,num_meas=5,delay_between_measurements=10*us)
    #build_impulses_for_impulse_odmr(t1=20*us, t2_start=5*us, t2_end=20*us, t2_step=1*us, t3=20*us,t4=10*us,t5=20*us,t6=10*us,t7=10*us,t8=20*us,num_meas=5,delay_between_measurements=10*us)