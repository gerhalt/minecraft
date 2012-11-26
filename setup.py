#!/usr/bin/python
'''

Building the C extension

'''

from distutils.core import setup, Extension

minecraft_base = Extension('minecraft',
                           sources = ['minecraft.c', 'chunk.c'])

setup (name = 'Minecraft',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [
            Extension("minecraft", ["minecraft.c"])
       ])

