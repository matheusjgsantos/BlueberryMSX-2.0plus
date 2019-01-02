#!/usr/bin/env python3

import sys
import os
import glob
import subprocess
import shlex
import configparser
from google import google
import xml.etree.ElementTree as ET

try:
    import pygame
except ImportError:
    print("ERROR: Can't find Pygame!\nPlease install with 'pip install pygame', use your package\nmanager on linux or RaspberryPi, or see the Pygame docs:\nhttp://www.pygame.org/wiki/GettingStarted")
    sys.exit()


white = (255,255,255)
grey = (100,100,100)

def flush_input():
    try:
        import msvcrt
        while msvcrt.kbhit():
            msvcrt.getch()
    except ImportError:
        import sys, termios
        termios.tcflush(sys.stdin, termios.TCIOFLUSH)

class Game(object):
    def __init__(self, font, rom="", emulator="advmame", title="", imgpath="", find_image=None, max_size=-1, extra_paths=[]):
        if imgpath:
            filename, _ = os.path.splitext(os.path.basename(imgpath))
            print (filename)
            if "_" in filename:
                rom, title = filename.split("_", 1)
            else:
                rom = title = filename
        elif rom:
            imgpath = find_image(rom, emulator, extra_paths)
        if title:
            self.title = title
        else:
            self.title = rom
        self.cmdline = emulator + str(next(iter(extra_paths), '')) + rom 
        #print(self.cmdline)
        try:
            self.image = pygame.image.load(imgpath).convert(24)
            self.rect = self.image.get_rect()
            if max_size > 0:
                self.rescale(max_size)
        except pygame.error:
            self.image = None
        self.label = font.render(self.title, 1, (255, 100, 100))
        self.label_size = font.size(self.title)

    def __lt__(self, other):
        return self.title < other.title

    def rescale(self, max_size):
        w = self.rect.width
        h = self.rect.height
        ar = w * 1.0 / h
        if ar > 1:
            if w > max_size:
                h = int(max_size / ar)
                w = max_size
        else:
            if h > max_size:
                w = int(max_size * ar)
                h = max_size
        self.image = pygame.transform.smoothscale(self.image, (int(w*0.9), int(h*0.9)))
#        self.image = pygame.transform.scale(self.image, (w, h))
        self.rect = self.image.get_rect() 

    def draw(self, screen, x, y, font_y):
        if self.image is not None:
            self.rect.centerx = x
            self.rect.centery = y
#            screen.blit(pygame.transform.scale(self.image, (int(self.rect.width*0.9), int(self.rect.height*0.9))), self.rect)
            screen.blit(self.image, self.rect)
        w, h = self.label_size
        r = pygame.Rect((0, font_y, w, h))
        r.centerx = x
        screen.blit(self.label, r)

    @property
    def size(self):
        if self.image is not None:
            return self.rect.width, self.rect.height
        return 0, 0

    def run(self):
        #print (self.cmdline)
        os.system('tput civis')
        os.system(self.cmdline + " /machine zemmix > /dev/null 2> /dev/null")
        flush_input()
        #subprocess.call(self.cmdline)


class Menu(object):
    image_paths = ["."]
    konami_code_key_list = ["up", "up", "down", "down", "left", "right", "left", "right", "b", "a"]

    def __init__(self, mainmenu_name = "rexmenu", alternate_config=None):
        self.w = 1024
        self.h = 768
        self.usable_h = self.h
        self.highlight_size = 10
        self.grid_spacing = 5
        self.highlight_color = (50, 50, 255)
        self.name_spacing = 5
        self.rows = 0
        self.cols = 4
        self.visible_rows = 0
        self.title_height = 0
        self.title_image = None
        self.title_image_rect = pygame.Rect((0, 0, 0, 0))
        self.first_visible = 0
        self.num_visible = 0
        self.screen = None
        self.clock = pygame.time.Clock()
        self.games = []
        self.font = None
        self.mainmenu = mainmenu_name
        self.thumbnail_size = 220
        self.windowed = False
        self.clear_screen = True
        self.wrap_menu = True
        self.konami_code = "exit"

        # key definitions set during config file parsing
        self.quit_keys = None
        self.run_keys = None
        self.up_keys = None
        self.down_keys = None
        self.left_keys = None
        self.right_keys = None
        self.konami_index = 0
        self.cfg = self.find_cfg(alternate_config)

    def find_cfg(self, alternate_config=None):
        if alternate_config is not None:
            order = [alternate_config]
        else:
            order = ["~/.rexmenu", "/etc/rexmenu.cfg", "rexmenu.cfg"]
        fh = None
        home = os.path.expanduser("~")
        for cfg in order:
            if "~" in cfg:
                cfg = cfg.replace("~", home)

            try:
                fh = open(cfg, "r")
                break
            except IOError:
                pass
        if fh is not None:
            return self.parse_cfg(fh)
        else:
            raise RuntimeError("Configuration file not found. Tried: %s" % (", ".join(order)))

    def setup(self):
        self.font = pygame.font.Font("tvN 즐거운이야기 Bold.ttf", 20)
        if self.cfg is not None:
            self.parse_games_cfg(self.cfg)
        else:
            self.parse_games()
        if self.windowed:
            flags = 0
        else:
            flags = pygame.FULLSCREEN
            info = pygame.display.Info()
            self.w = info.current_w
            self.h = info.current_h
        self.screen = pygame.display.set_mode((self.w, self.h), flags)
        pygame.mouse.set_visible(False)

    def find_image(self, rom, emulator, extra_paths=[]):
        # alternate locations and filenames
        alternates = [rom]
        base = os.path.basename(rom)
        if base != rom:
            alternates.append(rom)
        r, _ = os.path.splitext(rom)
        if r != rom:
            alternates.append(r)

        # loop through all search directory combos looking for PNG or JPG files
        # that match the game name
        search_paths = list(extra_paths)
        search_paths.extend(self.image_paths)
        for base in alternates:
            for ext in [".png", ".jpg"]:
                image = base + ext
                if not os.path.isabs(image):
                    for path in search_paths:
                        filename = os.path.join(path, image)
                        if os.path.exists(filename):
                            return filename
                else:
                    if os.path.exists(image):
                        return image
        return None

    def mainmenu_title(self, cfg, keyword, value):
        self.load_title_image(value)

    def mainmenu_image_path(self, cfg, keyword, value):
        self.image_paths = shlex.split(value)

    def get_keys(self, keysyms):
        keys = []
        for k in keysyms.split():
            for c in [k, k.upper(), k.lower()]:
                a = "K_%s" % c
                if hasattr(pygame, a):
                    keys.append(getattr(pygame, a))
                    break
        return keys

    key_defaults = {
        "run": "Z X LSHIFT LCTRL SPACE RETURN 1 2 3 4",
        "quit": "ESCAPE",
        "up": "UP",
        "down": "DOWN",
        "left": "LEFT",
        "right": "RIGHT",
        "a": "1",
        "b": "2",
    }

    text_defaults = {
        "konami code": "konami_code",
    }

    int_defaults = {
        "window width": "w",
        "window height": "h",
        "thumbnail size": "thumbnail_size",
        "highlight size": "highlight_size",
        "grid spacing": "grid_spacing",
        "name spacing": "name_spacing",
    }

    bool_defaults = {
        "windowed": "windowed",
        "clear screen": "clear_screen",
        "wrap menu": "wrap_menu",
    }

    def parse_cfg_mainmenu(self, c):
        possible = c.options(self.mainmenu)
        for keyword in possible:
            func = "mainmenu_%s" % keyword.replace(" ", "_")
            if hasattr(self, func):
                func = getattr(self, func)
                value = c.get(self.mainmenu, keyword)
                func(c, keyword, value)
        for key, keysyms in self.key_defaults.items():
            if c.has_option(self.mainmenu, key):
                keysyms = c.get(self.mainmenu, key)
            key_list = self.get_keys(keysyms)
            setattr(self, "%s_keys" % key, key_list)
        for keyword, attr_name in self.text_defaults.items():
            if c.has_option(self.mainmenu, keyword):
                value = c.get(self.mainmenu, keyword)
                setattr(self, attr_name, value)
        for keyword, attr_name in self.int_defaults.items():
            if c.has_option(self.mainmenu, keyword):
                value = c.getint(self.mainmenu, keyword)
                setattr(self, attr_name, value)
        for keyword, attr_name in self.bool_defaults.items():
            if c.has_option(self.mainmenu, keyword):
                value = c.getboolean(self.mainmenu, keyword)
                setattr(self, attr_name, value)

    def parse_cfg(self, fh):
        c = configparser.ConfigParser()

        # option names are filenames, so we have to change the default to
        # prevent lower casing of all names.
        c.optionxform = str

        c.readfp(fh, "rexmenu.cfg")
        if self.mainmenu in c.sections():
            self.parse_cfg_mainmenu(c)
        return c

    def parse_games_cfg(self, c):
        emulators = [e for e in c.sections() if e != self.mainmenu]
        for emulator in emulators:
            extra_image_path = []
            for rom in c.options(emulator):
                name = c.get(emulator, rom)
                if rom == "title from name":
                    for r in name.split():
                        game = Game(self.font, r, emulator, find_image=self.find_image, max_size=self.thumbnail_size)
                        self.games.append(game)
                elif rom == "image path":
                    extra_image_path = shlex.split(name)
                elif rom == "allroms":
                    roms = os.listdir(extra_image_path[0])
                    index = 0
                    for rom in roms:
                        if (os.path.splitext(rom)[1] == '.zip'):
                            game = Game(self.font, rom, emulator, os.path.splitext(rom)[0], find_image=self.find_image, max_size=self.thumbnail_size, extra_paths=extra_image_path)
                            self.games.append(game)
                        index = index + 1
                        if (index > 100):
                            break
                elif rom == "xml":
                    index = 0
                    root = ET.parse(name)
                    for software in root.iter('software'):
                        rom = software.attrib['name'] + '.zip'
                        if (os.path.exists(extra_image_path[0] + rom) and software[0].text.find("Kor") > 0 and software[0].text.find("Alt") < 0):
                            game = Game(self.font, rom, emulator, software[0].text, find_image=self.find_image, max_size=self.thumbnail_size, extra_paths=extra_image_path)
                            self.games.append(game)
                            index = index + 1
                            #print ("searching... %d\r" % index)
                else:
                    game = Game(self.font, rom, emulator, name, find_image=self.find_image, max_size=self.thumbnail_size, extra_paths=extra_image_path)
                    self.games.append(game)
        self.games.sort()

    def parse_games(self, path="*.png"):
        for filename in glob.glob(path):
            if filename == "title.png":
                self.load_title_image(filename)
                continue
            game = Game(self.font, emulator="advmame", imgpath=filename)
            self.games.append(game)
        self.games.sort()

    def load_title_image(self, filename):
        if not os.path.isabs(filename):
            filename = self.find_image(filename, "")
        if filename:
            self.title_image = pygame.image.load(filename)

    def make_grid(self):
        w = 60
        h = 60
        for game in self.games:
            gw, gh = game.size
            if gw > w:
                w = gw
            if gh > h:
                h = gh
        _, self.font_height = self.font.size("MMMM")
        if self.title_image is not None:
            r = self.title_image.get_rect()
            self.title_image_rect = pygame.Rect((0, 0, r.width, r.height))
            self.title_image_rect.centerx = self.w / 2
        else:
            self.title_image_rect = pygame.Rect((0, 0, 0, 0))
        self.title_height = 10 + self.title_image_rect.height
        w += self.grid_spacing + self.highlight_size 
        h += self.grid_spacing + self.highlight_size + self.font_height + self.name_spacing - 20
        print (w, h)
        self.usable_h = self.h - self.title_height
        self.cols = int(self.w / w)
        self.rows = int((len(self.games) + self.cols - 1) / self.cols)
        self.grid_size = (w, h)
        self.visible_rows = int(self.usable_h / self.grid_size[1])
        self.num_visible = self.visible_rows * self.cols
        self.center_offset = (w/2, h/2)
        self.x_offset = (self.w - (w * self.cols)) / 2
        self.y_offset = self.title_height + (pygame.display.Info().current_h - self.title_height - h * self.visible_rows)/2
        print (self.y_offset, self.rows, h, pygame.display.Info().current_h)

    def get_grid_pos(self, index):
        r, c = divmod(index - self.first_visible, self.cols)
        x = self.x_offset + (c * self.grid_size[0])
        y = self.y_offset + (r * self.grid_size[1])
        return x, y

    def get_center(self, index):
        x, y = self.get_grid_pos(index)
        x += self.center_offset[0]
        y += self.center_offset[1]
        return x, y

    def get_highlight_rect(self, index):
        x, y = self.get_grid_pos(index)
        w, h = self.grid_size
        return x, y, w, h

    def get_font_pos(self, index):
        x, y = self.get_grid_pos(index)
        x += self.center_offset[0]
        y += self.grid_size[1] - self.highlight_size - self.font_height
        return x, y

    def is_visible(self, index):
        return index >= self.first_visible and index < self.first_visible + self.num_visible

    def reset_konami(self):
        self.konami_index = 0

    def peek_konami(self, keyword):
        return keyword == self.konami_code_key_list[self.konami_index]

    def check_konami(self, keyword):
        if self.peek_konami(keyword):
            self.konami_index += 1
        else:
            self.konami_index = 0
        return self.konami_index == len(self.konami_code_key_list)

    def do_konami(self):
        self.reset_konami()
        action = self.konami_code
        if action == "exit":
            sys.exit(1)
        else:
            self.restart(action)
    def do_up(self, game_index):

        return game_index

    def restart(self, *extra_args):
        python = sys.executable
        args = [sys.argv[0]]
        for arg in extra_args:
            args.append(str(arg))
        os.execl(python, python, *args)

     
            
    def show(self, game_index=0):
        """Main routine to check for user input and drawing the menu

        """
        self.setup()
        self.make_grid()
        done = False
        last_index = -1
        num_games = len(self.games)
        if game_index >= num_games:
            game_index = num_games - 1
        self.reset_konami()
        konami = False
        joysticks = []
        for i in range(0, pygame.joystick.get_count()):
            joysticks.append(pygame.joystick.Joystick(i))
            joysticks[-1].init()
        run = False
        j = None 
        if joysticks != []:
            j = pygame.joystick.Joystick(0)	
        while not done:
            if j != None:
                updateData(j)
                doActions()
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    done = True
                if event.type == pygame.KEYDOWN:
                    if not event.key in self.run_keys:
                        run = False
                    if event.key in self.quit_keys:
                        done = True
                        os.system('tput cvvis')
                    elif event.key in self.run_keys and run != True:
                        if event.key in self.b_keys and self.peek_konami("b"):
                            konami = self.check_konami("b")
                        elif event.key in self.a_keys and self.peek_konami("a"):
                            konami = self.check_konami("a")
                        else:
                            #print (game_index)
                            game = self.games[int(game_index)]
                            game.run()
                            self.draw_menu(game_index)

                            # restart the program to re-initialize pygame
                            #self.restart(game_index)

                            # only reaches here on an error.
                        run = True
                    elif event.key in self.up_keys:
                        game_index -= self.cols
                        if game_index < 0:
                            if self.wrap_menu:
                                game_index += self.cols * self.rows
                                if game_index >= num_games:
                                    game_index -= self.cols
                            else:
                                game_index += self.cols
                    elif event.key in self.down_keys:
                        konami = self.check_konami("down")
                        game_index += self.cols
                        if game_index >= num_games:
                            # check for partial last row
                            if game_index < self.cols * self.rows:
                                game_index = num_games - 1
                            elif self.wrap_menu:
                                game_index = game_index % self.cols
                            else:
                                game_index -= self.cols
                    elif event.key in self.left_keys:
                        konami = self.check_konami("left")
                        game_index -= 1
                        if game_index < 0:
                            if self.wrap_menu:
                                game_index = num_games - 1
                            else:
                                game_index = 0
                    elif event.key in self.right_keys:
                        konami = self.check_konami("right")
                        game_index += 1
                        if game_index >= num_games:
                            if self.wrap_menu:
                                game_index = 0
                            else:
                                game_index = num_games - 1
                    elif event.key == pygame.K_y:
                        pygame.draw.circle(self.screen, self.highlight_color, (self.w-10, 10), 10)
                        pygame.display.flip()
                        s =  google.search("youtube msx1 " + self.games[game_index].title, 1)
                        pygame.draw.circle(self.screen, (255,0,0), (self.w-10, 10), 10)
                        pygame.display.flip()
                        if (len(s) > 0):
                            os.system("./youtube " + s[0].link)
                    if konami:
                        self.do_konami()
                        konami = False

            if not done and game_index != last_index:
                # adjust the first visible row depending on the direction of
                # the scroll
                while not self.is_visible(game_index):
                    if game_index > last_index:
                        self.first_visible += self.cols
                    else:
                        self.first_visible -= self.cols
                self.draw_menu(game_index)
                last_index = game_index

    def draw_menu(self, game_index):
        """Redraw the entire screen to make the specified game index visible
        """
        self.screen.fill((0,0,0))
        if self.title_image is not None:
            self.screen.blit(self.title_image, self.title_image_rect)

        # draw up arrow if the first row isn't on screen
        size = self.h * 0.05
        if self.first_visible > 0:
            pygame.draw.polygon(self.screen, self.highlight_color, [(size/2, 0), (size, size), (0, size)])

        # draw down arrow if the last row isn't on screen
        last_visible = (self.first_visible / self.cols) + self.visible_rows - 1
        if last_visible < self.rows - 1:
            pygame.draw.polygon(self.screen, self.highlight_color, [(self.w-size/2, self.h), (self.w-size, self.h - size), (self.w, self.h - size)])

        # draw game thumbnails for those games currently visible
        for i, game in enumerate(self.games):
            if self.is_visible(i):
                cx, cy = self.get_center(i)
                #print (i, cx, cy)
                if game_index == i:
                    r = self.get_highlight_rect(i)
                    # pygame.draw.rect leaves blocky corners with wide lines
                    self.screen.fill(self.highlight_color, r)
                    s = self.highlight_size + 50  
                    r = (r[0] + s, r[1] + s, r[2] - s - s, r[3] - s - s)
                    self.screen.fill((0,0,0), r)
                x, y = self.get_font_pos(i)
                game.draw(self.screen, cx, cy - 10, y)
        pygame.display.flip()
#        self.clock.tick(5)

controls = [0,0,0,0,0,0,0,0,0,0,0,0]
old_controls = controls[:]

def updateData(joystick):
    global old_controls, controls
    old_controls = controls[:]
    controls = [joystick.get_axis(1) < 0, joystick.get_axis(1) > 0, joystick.get_axis(0) < 0, joystick.get_axis(0) > 0, 
        joystick.get_button(3), joystick.get_button(4), joystick.get_button(2), joystick.get_button(3), 
        joystick.get_button(4), joystick.get_button(5), joystick.get_button(8), joystick.get_button(9)]

actions = [pygame.K_UP, pygame.K_DOWN, pygame.K_LEFT, pygame.K_RIGHT, pygame.K_RETURN, pygame.K_RETURN, pygame.K_RETURN, pygame.K_RETURN, None, None, None, pygame.K_RETURN  ]
 
def doActions():
    global controls, old_controls
    for i in range(0,len(controls)):
        if old_controls[i] == 0 and controls[i] > 0 and actions[i] != None:
            pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=actions[i]))


    
if __name__ == "__main__":
    # argument to the script is an index number for the initial game to be
    # highlighted.
    game_index = 0
    alternate_config = None
    if len(sys.argv) > 1:
        try:
            game_index = int(sys.argv[1])
        except:
            alternate_config = sys.argv[1]
    pygame.joystick.init()
    # parse menu before initializing pygame so we can check the config file
    menu = Menu(alternate_config=alternate_config)
    if menu.clear_screen:
        subprocess.call('clear', shell=True)

    pygame.init()
    pygame.mouse.set_visible(False)
    menu.show(game_index)
    pygame.quit()

