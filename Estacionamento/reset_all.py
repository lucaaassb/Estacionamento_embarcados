import RPi.GPIO as GPIO
from time import sleep

# Define o padrao de numeracao das portas como BCM
# A outra opcap e GPIO.BOARD para usar o numero dos pinos fisicos da placa
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

output_pins = [
	22, 26, 19, 18, 27, 23, 24, 10, 25, 12, 17,
	13, 6, 5, 20, 8, 16, 21,
	14, 15, 7, 1, 4, 9, 11
]

for pin in output_pins:
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)
