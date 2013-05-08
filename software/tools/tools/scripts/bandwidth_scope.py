#!/usr/bin/python
import os,sys
import lcm
import time
from lcm import LCM
import math
import numpy  as np
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab

from threading import Thread

home_dir =os.getenv("HOME")
#print home_dir
sys.path.append(home_dir + "/drc/software/build/lib/python2.7/site-packages")
sys.path.append(home_dir + "/drc/software/build/lib/python2.7/dist-packages")

from drc.bandwidth_stats_t import bandwidth_stats_t
########################################################################################

def timestamp_now (): return int (time.time () * 1000000)

class SensorData(object):
    def __init__(self, nfields):
        self.nfields = nfields
        self.reset()
    def append(self, utime,v_in ):
        np_v = np.array(v_in)
        if (self.v.shape[1] != np_v.size):
          self.nfields = np_v.size
          self.reset()
        np_utimes = np.array( (utime - first_utime)/1000000.0 )
        self.utimes = np.vstack((self.utimes , np_utimes ))
        self.v = np.vstack((self.v , np_v ))
    def reset(self):
        # no sure how to support initialising a Nx0 array, so I'm initing a Nx1 array and skipping 1st row:
        self.utimes=np.array([0]) 
        self.v=np.zeros((1, self.nfields))

        
def reset_all():
  msgs.reset(); queued_MB.reset(); cumqueued_MB.reset()

  
def plot_data():
  global last_utime
  global channels
  front_block =0 # offset into the future (to ensure no recent data not viewed)
  #print "len of channels: %d" %  len(channels)
  cols = 'rgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmcrgbkmc'
  if ( len(msgs.utimes) >1):
    plt.figure(1)
    ############################################################
    ax1.cla()
    j=-1
    for i in range(0,len(channels)): 
      if(cumqueued_MB.v[-1,i]>0):
        if(j==-1):
          ax1.fill_between( cumqueued_MB.utimes[1:,0][:], 0 , cumqueued_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        else:
          ax1.fill_between( cumqueued_MB.utimes[1:,0][:], cumqueued_MB.v[1:,j][:] , cumqueued_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        j=i
        ax1.plot(cumqueued_MB.utimes[1:], np.transpose(cumqueued_MB.v[1:,i]),color=cols[i] ,  linewidth=1,label=channels[i])
    ax1.set_ylabel('Cum MB Queued [' + stats_channel +']');  ax1.grid(True)
    ax1.legend(loc=2,prop={'size':10})
    ax1.set_xlim( (last_utime - plot_window - first_utime)/1000000 , (last_utime + front_block - first_utime)/1000000 )
    ax1.set_ylim( bottom=0)
    
    ############################################################
    ax2.cla()
    j=-1
    for i in range(0,len(channels)): 
      if(cumsent_MB.v[-1,i]>0):
        if(j==-1):
          ax2.fill_between( cumsent_MB.utimes[1:,0][:], 0 , cumsent_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        else:
          ax2.fill_between( cumsent_MB.utimes[1:,0][:], cumsent_MB.v[1:,j][:] , cumsent_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        j=i
        ax2.plot(cumsent_MB.utimes[1:], np.transpose(cumsent_MB.v[1:,i]),color=cols[i] ,  linewidth=1,label=channels[i])
    ax2.set_ylabel('Cum MB Sent [' + stats_channel +']');  ax2.grid(True)
    ax2.legend(loc=2,prop={'size':10})
    ax2.set_xlim( (last_utime - plot_window - first_utime)/1000000 , (last_utime + front_block - first_utime)/1000000 )
    ax2.set_ylim( bottom=0)

    ############################################################
    ax3.cla()
    j=-1
    for i in range(0,len(channels)): 
      if(cumreceived_MB.v[-1,i]>0):
        if(j==-1):
          ax3.fill_between( cumreceived_MB.utimes[1:,0][:], 0 , cumreceived_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        else:
          ax3.fill_between( cumreceived_MB.utimes[1:,0][:], cumreceived_MB.v[1:,j][:] , cumreceived_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        j=i
        ax3.plot(cumreceived_MB.utimes[1:], np.transpose(cumreceived_MB.v[1:,i]),color=cols[i] ,  linewidth=1,label=channels[i])
    ax3.set_ylabel('Cum MB Received [' + stats_channel +']');  ax3.grid(True)
    ax3.legend(loc=2,prop={'size':10})
    ax3.set_xlim( (last_utime - plot_window - first_utime)/1000000 , (last_utime + front_block - first_utime)/1000000 )
    ax3.set_ylim( bottom=0)
    
    ############################################################
    ax4.cla()
    j=-1
    for i in range(0,len(channels)): 
      if(cumreceived_MB.v[-1,i]>0):
        if(j==-1):
          ax4.fill_between( cumreceived_MB.utimes[1:,0][:], 0 , cumreceived_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        else:
          ax4.fill_between( cumreceived_MB.utimes[1:,0][:], cumreceived_MB.v[1:,j][:] , cumreceived_MB.v[1:,i][:],color=cols[i] , label=channels[i]) 
        j=i
        ax4.plot(cumreceived_MB.utimes[1:], np.transpose(cumreceived_MB.v[1:,i]),color=cols[i] ,  linewidth=1,label=channels[i])
    ax4.set_ylabel('Cum MB Received [' + stats_channel +']');  ax4.grid(True)
    ax4.set_xlim( (last_utime - plot_window - first_utime)/1000000 , (last_utime + front_block - first_utime)/1000000 )
    ax4.set_ylim( bottom=0)

  plt.plot()
  plt.draw()
  

# Microstrain INS/IMU Sensor:
def on_bw(channel, data):
  m = bandwidth_stats_t.decode(data)
  msgs.append(m.utime,m.queued_msgs)
  
  this_queued_MB= tuple([x/1024.0/1024.0 for x in m.queued_bytes])
  queued_MB.append(m.utime,this_queued_MB)
  cumqueued_MB.append(m.utime,np.cumsum(this_queued_MB))

  this_sent_MB=tuple([x/1024.0/1024.0 for x in m.sent_bytes]) 
  #sent_MB.append(m.utime,this_sent_MB)
  cumsent_MB.append(m.utime,np.cumsum(this_sent_MB))

  this_received_MB=tuple([x/1024.0/1024.0 for x in m.received_bytes]) 
  #received_MB.append(m.utime,this_received_MB)
  cumreceived_MB.append(m.utime,np.cumsum(this_received_MB))
  
  global stats_channel
  stats_channel = channel
  global channels
  channels = m.channels
  global last_utime
  if (m.utime < last_utime):
    print "out of order data, resetting now %s | last %s"   %(m.utime,last_utime)
    reset_all()
  last_utime = m.utime
  
#################################################################################
channels=[]
stats_channel=''

lc = lcm.LCM()
print "started"
last_utime=0
first_utime=0
plot_window=30*1000000 #3sec
msgs = SensorData(17); queued_MB = SensorData(17);
cumqueued_MB = SensorData(17); cumsent_MB = SensorData(17); cumreceived_MB = SensorData(17);

left, bottom, width, height =0.07, 0.07, 0.395, 0.395
box_ul = [left, 2*bottom+height, width, height]
box_ur = [2*left+width, 2*bottom+height, width, height]
box_ll = [left, bottom, width, height]
box_lr = [2*left+width, bottom, width, height]


fig1 = plt.figure(num=1, figsize=(14, 10), dpi=80, facecolor='w', edgecolor='k')
ax1 = fig1.add_axes(box_ul)
ax2 = fig1.add_axes(box_ur)
ax3 = fig1.add_axes(box_ll)
ax4 = fig1.add_axes(box_lr)

plt.interactive(True)
plt.plot()
plt.draw()

def lcm_thread():
  sub1 = lc.subscribe("BASE_BW_STATS", on_bw) # required
  sub1 = lc.subscribe("ROBOT_BW_STATS", on_bw) # required
  
  while True:
    ## Handle LCM if new messages have arrived.
    lc.handle()

  lc.unsubscribe(sub1)

t2 = Thread(target=lcm_thread)
t2.start()

time.sleep(3) # wait for some data- could easily remove

plot_timing=0.1 # time between updates of the plots - in wall time
while (1==1):
  time.sleep(plot_timing)
  tic_ms = float(round(time.time() * 1000))
  plot_data()
  toc_ms = float(round(time.time() * 1000))
  dt_sec = (toc_ms - tic_ms)/1000
  print "drawing time: %f" %(dt_sec)
