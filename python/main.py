import paho.mqtt.client as mqtt
import json
from easy_trilateration.model import *
from easy_trilateration.least_squares import easy_least_squares
import numpy
import matplotlib
import matplotlib.pyplot as plt

distmap = {}

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe("devices/+/telemetry", 2)

def on_message(client, userdata, msg):
    data = msg.payload
    distmap[msg.topic] = float(data)
    if(len(distmap) == 3):
        print(distmap)
        dd = numpy.empty(3, dtype=object)
        for key in distmap:
            match key:
                case "devices/a/telemetry":
                    dd[0]=distmap[key]
                case "devices/b/telemetry":
                    dd[1]=distmap[key]
                case "devices/c/telemetry":
                    dd[2]=distmap[key]

        arr = [[0, 0, dd[0]],
               [4.5, 0, dd[2]],
               [0, -7.7, dd[1]]]

        circle_arr=[Circle(arr[0][0],arr[0][1], arr[0][2]),
                    Circle(arr[1][0], arr[1][1], arr[1][2]),
                    Circle(arr[2][0], arr[2][1], arr[2][2])]
        result, meta = easy_least_squares(circle_arr)
        plt.cla()
        circle1 = plt.Circle((arr[0][0],arr[0][1]), arr[0][2], color='r', fill=False)
        circle2 = plt.Circle((arr[1][0], arr[1][1]), arr[1][2], color='blue',fill=False)
        circle3 = plt.Circle((arr[2][0], arr[2][1]), arr[2][2], color='g', fill=False)
        plt.scatter(arr[0][0],arr[0][1], color='r')
        plt.scatter(arr[1][0], arr[1][1], color='blue')
        plt.scatter(arr[2][0], arr[2][1],  color='g')
        plt.scatter(result.center.x, result.center.y, color='orange')
        plt.gca().add_patch(circle1)
        plt.gca().add_patch(circle2)
        plt.gca().add_patch(circle3)
        plt.xlim(-10, 10)
        plt.ylim(-10, 10)
        plt.gca().set_aspect('equal', adjustable='box')

        plt.pause(0.05)


if __name__ == '__main__':
    plt.show()
    client = mqtt.Client("mqtt-test")  # client ID "mqtt-test"
    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set("user", "pass")
    client.connect('127.0.0.1', 1883)
    client.loop_forever()  # Start networking daemon