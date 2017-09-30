#!/home/shumin/local/python-3.6.0/bin/python3

##################################################################################
#
# 	Ex) python3 Makefile.py suffix True True
#
##################################################################################

import os
import sys
from multiprocessing import Process, Pool, Lock, Array
#from threading import Lock

print("****************** Making Script Files *****************")

#program_list_spec = ['bzip2', 'gcc', 'astar', 'mcf', 'libquantum', 'sjeng', 'cactusADM', 'calculix']
program_list_spec = ['bzip2', 'astar', 'mcf', 'libquantum', 'sjeng', 'cactusADM', 'calculix']
program_list_parcec = ['blackscholes', 'freqmine', 'fluidanimate']
base_script_name = 'ARMv8O3Sim'
numberOfProcess = 10
suffix = sys.argv[1] # file name suffix
suffix2 = sys.argv[2] # front-end optimizations
suffix3 = sys.argv[3] # back-end optimizations

test_list = [i for i in range(1,11)]

def makeScriptFIles():
	for i in program_list_parcec:
		os.system("echo Hello" + " " + i)
		os.system("cp "+base_script_name + " " + base_script_name + "-" + i)

def executeBenchmark():
	for i in (program_list_spec + program_list_parcec):
		os.system("./" + base_script_name + "-" + i + " " + suffix + " " + suffix2 + " " + suffix3 + " &")

def foo(name, lock, arr):

	lock.acquire()
	try:
		if(len(arr) == 0):
			print("The list is empty")
			return -1

		del arr[0] 		# remove the first element of list
		print("My Number : ", arr[0])
		print("My pid : ", os.getpid())
		print("My Parent pid : ", os.getppid())
	finally:
		lock.release()

def testMultiprocessing():

	global test_list
	lock = Lock()
	arr = Array('c', test_list)
	for i in range(0,numberOfProcess):
		p = Process(target=foo, args=('shumin', lock, arr))
		p.start()
	p.join()

	print(arr[:])

def testPool():
	p = Pool(3)
	p.map(foo, range(0,10))


if(__name__ == '__main__'):
	executeBenchmark()
#	testPool()
