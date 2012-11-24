#!/usr/bin/python
'''

Building the C extension

'''

from distutils.core import setup, Extension

minecraft_base = Extension('minecraft',
                           sources = ['minecraft.c'])

setup (name = 'Minecraft',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [minecraft_base])

