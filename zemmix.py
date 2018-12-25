#!/usr/bin/env python
# -*- coding: utf-8 -*-
 
# Modules
import sys
import os
import pygame
from pygame.locals import *
import numpy as np
import pygameMenu
from pygameMenu.locals import *
import subprocess
import pgzero
import ptext
import GIFImage
from collections import namedtuple
import RPi.GPIO as GPIO
from pathlib import Path

# Colors
# ---------------------------------------------------------------------
alive = (251,224,10)
dead = (53,83,87)
menu_color = (170, 170, 170)
menu_color_title = (170,65,50)
color_selected = (170,65,50)
font_color = (0, 0, 0)
text_color = (0, 0, 0)

BLACK = (  0,   0,   0)
WHITE = (255, 255, 255)
BLUE  = (  0,   0, 255)
GREEN = (  0, 255,   0)
RED   = (255,   0,   0)
YELLOW= (255, 255,   0)

HIDDEN_CURSOR = "tput civis"
ZEMIX_BOLD = "tvN 즐거운이야기 Bold.ttf"
gpio_check = 0;
diskfiles = []
diskfiles.append(Path("/var/log/sda"))
diskfiles.append(Path("/var/log/sdb"))
diskfiles.append(Path("/var/log/sdc"))
diskfiles.append(Path("/var/log/sdd"))

def check_rompack():
    value = GPIO.input(27)
    if (value == 0):
        os.system("./bluemsx-pi /romtype1 msxbus > /dev/null 2>&1")
        os.system(HIDDEN_CURSOR)
    return True if value == 0 else False


# Global variables
# ---------------------------------------------------------------------
_=os.system('clear')
APP_FOLDER = os.path.dirname(os.path.realpath(sys.argv[0]))
os.chdir(APP_FOLDER)
GPIO.setmode(GPIO.BCM)
GPIO.setup(27, GPIO.OUT)
GPIO.output(27, True)
if (check_rompack()):
    gpio_check = -1
frame_rate = 10
pygame.mixer.init()
pygame.mixer.music.load("01. The Adventure.mp3")
#pygame.mixer.music.play()
print (os.getcwd())
btn_effect = pygame.mixer.Sound('sound/button.wav')
btn_effect.set_volume(0.5)
run_effect = pygame.mixer.Sound('sound/chime.wav')
run_effect.set_volume(0.5)

pygame.display.init()
pygame.mouse.set_visible(False)
width, height = (pygame.display.Info().current_w, pygame.display.Info().current_h)
print ("Framebuffer size: %d x %d" % (width, height))
screen = pygame.display.set_mode((width, height))
screen.fill((255,255,255))
clock = pygame.time.Clock()
paused = False
filenames = None
# ---------------------------------------------------------------------
# Functions
# ---------------------------------------------------------------------
def run_game(val, type):
    romtype = '' if type == '' else '/romtype1 ' + type
    run_effect.play() 
    os.system('./bluemsx-pi /machine zemmix /rom1 \"roms/' + val + '\" ' + romtype + ' > /dev/null 2>&1')
    os.system(HIDDEN_CURSOR)
    #    /rom1 roms/" + val)
def getfiles(dir):
	global filenames
	for dirpath, dirnames, filenames in os.walk(dir):
		gfs = filenames
	return filenames
def change_frame_rate(fr):
    global frame_rate
    frame_rate = fr
    
def menu_of_zemmixmini():
#    main_menu.disable()
#    main_menu.reset(1)
    screen.fill(WHITE)
    while True:
        clock.tick(frame_rate)
        events = pygame.event.get()
        for event in events:
            if event.type == KEYDOWN:
                if (event.key == K_ESCAPE):
                    exit()
#		screen.draw.text("재믹스미니 내장게임", centerx=width/2,  
#        main_menu.mainloop(evesnts)
#        draw_rects(rects, universe, screen)
#       if not paused:
#            evolve_universe()
        pygame.display.update()
# ---------------------------------------------------------------------

pygame.init()
pygame.font.init()
pygame.mouse.set_visible(False)

Game = namedtuple("Game", "title, rom, romtype, x, y")
gamelist = [Game('대마성','Legendly Knight (Korea).zip', '', 150, 480), 
    Game('아기공룡둘리', 'dooly_conv.rom', '', 150, 530),
    Game('사이보그Z', 'Cyborg Z (Zemina, 1991).zip', 'Konami', 150, 580),
    Game('스트리트마스터', 'street_master.zip', '', 150, 630),
    Game('강철로보캅', 'Gangcheol_Robocop.rom', '', 150, 680),
    Game('원시인', 'Wonsiin (KR).zip', '', 150, 480),
    Game('꾀돌이', 'koedoli.zip', '', 150, 530),
    Game('용의전설', '3DragonStory.rom', '', 150, 580),
    Game('어드벤처키드', 'Adventure Kid (Open Corp, 1992).zip', 'ASCII16', 150, 630),
    Game('띠띠빵빵', 'titipang.zip', '',150, 680)]

game_image = []
cursor_xy = []
index = 0
for a in gamelist:
    game_image.append(pygame.image.load('img/' + a.title + '.PNG').convert())
    cursor_xy.append((a.x-50 if index < 5 else width/2 + a.x - 50 , a.y))
    index = index + 1
cursor = pygame.image.load('img/coin.gif').convert()  
disk = pygame.image.load('img/disk.png').convert()  
#run_game("roms/" + a[0])
#exit()

# Main loop
pos = 0
while True:
    screen.fill(BLACK)
    clock.tick(frame_rate)
    ptext.draw("<재믹스미니 내장게임>", centerx=width/2, top=20, fontname=ZEMIX_BOLD, fontsize=80, color=RED, background=BLACK, antialias=True)
    index = 0
    for game in gamelist:
        if index > 4:
            x = game.x + width/2
        else:
            x = game.x 
        y = game.y
        if (index == pos):
            ptext.draw(game.title, left=x, top=y, fontname=ZEMIX_BOLD, fontsize=50, color=YELLOW, background=BLACK)
        else:
            ptext.draw(game.title, left=x, top=y, fontname=ZEMIX_BOLD, fontsize=50, color=WHITE, background=BLACK)
        index = index + 1
    screen.blit(cursor, cursor_xy[pos])
    screen.blit(pygame.transform.scale(game_image[pos], (200,300)), (width/2-100, 130))
    if (diskfiles[0].exists()):
        screen.blit(pygame.transform.scale(disk, (64, 64)), (10,10))
    if (diskfiles[1].exists()):
        screen.blit(pygame.transform.scale(disk, (64, 64)), (80,10))
    pygame.display.update()
    events = pygame.event.get()
    for event in events:
        if event.type == KEYDOWN:
            if (event.key == K_ESCAPE):
                os.system("tput cvvis")
                exit()
            if (event.key == K_LEFT and pos > 4):
                pos = pos - 5
                btn_effect.play()
            elif (event.key == K_RIGHT and pos < 5):
                pos = pos + 5
                btn_effect.play()
            elif (event.key == K_DOWN):
                btn_effect.play()
                if (pos < 9):
                    pos = pos + 1
                else:
                    pos = 0
            elif (event.key == K_UP):
                btn_effect.play()
                if (pos > 0):
                    pos = pos - 1
                else:
                    pos = 9
            elif (event.key == K_RETURN):
                run_game(gamelist[pos].rom, gamelist[pos].romtype)
            elif (event.key == K_F6):
                pygame.image.save(screen, "screen_shot.png")
            elif (event.key == K_j):
                os.system("./bluemsx-pi /machine zemmixj")
                os.system(HIDDEN_CURSOR)
            elif (event.key == K_t):
                os.system("./bluemsx-pi /machine zemmixtj")
                os.system(HIDDEN_CURSOR)
    if (gpio_check >= 0):
        gpio_check = gpio_check + 1
    elif (gpio_check == -1 and GPIO.input(27) == 1):
        gpio_check = 0
    if (gpio_check > 10):
        if (check_rompack()):
            gpio_check = -1
        else:
            gpio_check = 0
