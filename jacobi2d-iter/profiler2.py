#!/usr/bin/env python
import sys, os, subprocess, time
import datetime
#import numpy as np

def gennodelistfile(num_nodes):
	f = open('nodelist', 'w')
	f.write('group main ++shell /usr/bin/ssh\n')
	for i in range(num_nodes):
		f.write('host tarekc' + str(41+i) + '\n')
	f.flush()
	f.close()

logfile = './mylog'
lf = open(logfile, 'w')

	
nodes = [4]
#nodes = [20]
pkgPower = [38 + 2*i for i in range(12)]
dramPower = [80]#[8 + 2*i for i in range(6)][::-1]
print pkgPower
lf.write(str(pkgPower))
#sys.exit()
#logdir = '/home/sarood1/Work/201309_PowerDC/log/jacobi2d_20n'
logdir = '/home/sarood1/Work/201309_PowerDC/log/new_jacobi2d_72x72'

execdir = [
						'/home/sarood1/Work/201309_PowerDC/benchmarks/jacobi2d-iter/'	
           ]
runcmd = [
            'make test', 
         ]
appname = [
            'run'
          ]

#powerFile = '/dcsdata/home/alanger/workspace/fa12-cs598lvk-project/util/sample.log'

for i,app in enumerate(execdir):
	lf.write('profiling: ' + appname[i]+'\n')
	for scale in nodes:
		lf.write('running on ' + str(scale) + ' nodes\n')
		for pkgpower in pkgPower:
			for drampower in dramPower:
				lf.write('profiling at pkg power=' + str(pkgpower) + "w, dram power=" + str(drampower) + 'w\n')

				#set the power capping on all the nodes
				print 'setting power limts on all the nodes'
				for n in range(scale):
					print 'setting on',str(41+n),' pkg:',str(pkgpower),' mem: ',str(drampower)
					os.system('ssh tarekc' + str(41+n) + ' ~/fa12-cs598lvk-project/util/power_gov/setpowerlimits ' + str(pkgpower) + ' ' + str(drampower))
#					print 'ssh tarekc' + str(41+n) + ' ~/fa12-cs598lvk-project/util/power_gov/setpowerlimits ' + str(pkgpower) + ' ' + str(drampower)
				print 'power limts set on all the nodes'

				lf.write("changing to directory: " + execdir[i] + '\n')
				os.chdir(execdir[i] )
#				gennodelistfile(scale)
#				./charmrun +p120 ./jacobi2d 72000 600 10000 +setcpuaffinity +pemap 0-5 ++nodelist nodes40
				runcmd[i] = './charmrun +p'+ str(scale*6) +' ./jacobi2d 72000 600 10000 +setcpuaffinity +pemap 0-5 ++nodelist nodes40 |tee out'
				print 'running wavetemp on ' + str(scale) + ' nodes, pkg power ' + str(pkgpower) + 'w, dram power ' + str(drampower) + 'w\n'
				lf.write('running command: ' + runcmd[i]+'\n')
				print 'running command: ' + runcmd[i]+'\n'
				lf.flush()

				stime = str(time.mktime(datetime.datetime.now().timetuple()))

				os.system(runcmd[i])
				os.system('mkdir '+logdir+'/'+appname[i] + '_' + str(scale) + 'n_' + str(pkgpower) + '_' + str(drampower))
				os.system('mv out temp_pow.log.* '+logdir+'/'+appname[i] + '_' + str(scale) + 'n_' + str(pkgpower) + '_' + str(drampower)+'/')
				etime = str(time.mktime(datetime.datetime.now().timetuple()))
				lf.write('job wall times '+ stime +' '+ etime + '\n')
				lf.flush()
				time.sleep(15)
