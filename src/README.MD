**(English: Scrolldown for English | toki ike Inli li lon anpa pi toki pona)**

# sona pi ilo nena

kepeken sona ni la ilo nena li ken lukin e nena li pana e sitelen lon ma sitelen li pana e nimi tawa ilo sona.

## o pana e lipu sona `ilonena.bin` tawa ilo nena:

lipu sona `ilonena.bin` li lon. sina ken pana e sona ni tawa ilo nena kepeken nasin ni:

1. o kama jo e ilo `minichlink` tan ma ni: https://github.com/cnlohr/ch32fun/tree/master/minichlink
2. o weka e wawa tan ilo nena! sina pilin awen e nena "pana" la o pana sin e wawa tawa ilo nena! ni la ilo nena en ilo sona sina li kama wan. ilo nena sina li wile e sona.
3. o kepeken nimi ni lon ilo sona sina:

```
./minichlink -w /ma/pi/ilonena/src/firmware.bin 0x08000000 -b
```

## o ante e sona o pali e `ilonena.bin` sin:

1. o kama jo e ilo `riscv64-elf-gcc`
2. o kama jo e ilo `ch32fun` en ilo `rv003usb` kepeken nimi ni:

```
cd /ma/pi/wile/sina/
git clone https://github.com/cnlohr/ch32fun.git # mi kepeken nanpa ni: 46013996be1185e9479136fc87fa61f739bdf6cd
git clone https://github.com/cnlohr/rv003usb.git # mi kepeken nanpa ni: 35e0f688a9a041ed8cec37fcbb458df095dc0d79
```

3. o ante e lipu sona ni: `ali.c` en `ali.h`

4. o pali e `ilonena.bin` kepeken nimi ni:

```
export CH32FUN_PATH=/ma/pi/wile/sina/ch32fun/
export RV003USB_PATH=/ma/pi/wile/sina/rv003usb/
cd /ma/pi/ilonena/src/
make # ilo nena sina li wile e sona la nimi ni kin li ken pana e sona tawa ilo nena sina.
```


# Firmware of ilo nena

## To Flash Pre-built Firmware

1. Obtain `minichlink` here: https://github.com/cnlohr/ch32fun/tree/master/minichlink

2. Power off the ilo nena, hold the pana button (the yellow button on the rightmost column) and connect it to the computer again. ilo nena would enter USB firmware update mode.

3. Run the following command:

```
./minichlink -w /path/to/ilonena/src/firmware.bin 0x08000000 -b
```

## To Modify and Build New Firmware

1. Install riscv64-elf-gcc

2. Clone the ch32fun and rv003usb repo to somewhere you want:

```
cd /path/to/libaries/
git clone https://github.com/cnlohr/ch32fun.git # I've tested with 46013996be1185e9479136fc87fa61f739bdf6cd
git clone https://github.com/cnlohr/rv003usb.git # I've tested with 35e0f688a9a041ed8cec37fcbb458df095dc0d79
```

3. Make modification to the firmware source files (.c and .h files)

4. Build the firmware

```
export CH32FUN_PATH=/path/to/libaries/ch32fun/
export RV003USB_PATH=/path/to/libaries/rv003usb/
cd /path/to/ilonena/src
make # Would also flash if you are in USB firmware update mode
```
