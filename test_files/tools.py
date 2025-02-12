from datetime import datetime
from datetime import timedelta
import random

def time_series_data():
  now = datetime.now()
  data = []
  for _ in range(50):
    now = now + timedelta(minutes = 10)
    data.append([
      str(now),
      random.random()
    ])
  # print(data, flush=True)
  return data

def sum_items(arr):
  return sum(arr)

def multiply(a,b):
  return a * b

def return_immediate(x):
  # print(x, type(x))
  # print(x, flush=True)
  return x

def return_none():
  return None

def merge_two_dicts(x, y):
  z = x.copy()
  z.update(y)
  return z

def return_dict():
  x = {'size': 71589, 'min': -99.6654762642, 'max': 879.08351843}
  return x

def return_tuple():
  x = (1, 2, 3)
  return x

def causes_runtime_error():
  first = 1
  second = 2
  return first + secon

class Test:
  def __init__(self):
    self.v = "Test string"

  def t(self):
    return self.v

def return_class_object():
  return Test()

same_object = Test()
def return_same_object():
  return same_object
  
def call_callback(cb):
  cb("Hello")