#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

def DisplayGithub():
    print("\n")
    print("          Welcome to join me http://luotang.me                 ")
    print("          Welcome to join me https://github.com/wypx/libmsf    ")
    print("          Welcome to join me https://github.com/wypx/mobile    ")
    print("\n")

def DisplayLogo():
    print("                       .::::.                                  ")
    print("                     .::::::::.                                ")
    print("                    :::::::::::                                ")
    print("                 ..:::::::::::'                                ")
    print("               '::::::::::::'                                  ")
    print("                .::::::::::                                    ")
    print("           '::::::::::::::..                                   ")
    print("                ..::::::::::::.                                ")
    print("              ``::::::::::::::::                               ")
    print("               ::::``:::::::::'        .:::.                   ")
    print("              ::::'   ':::::'       .::::::::.                 ")
    print("            .::::'      ::::     .:::::::'::::.                ")
    print("           .:::'       :::::  .:::::::::' ':::::.              ")
    print("          .::'        :::::.:::::::::'      ':::::.            ")
    print("         .::'         ::::::::::::::'         ``::::.          ")
    print("     ...:::           ::::::::::::'              ``::.         ")
    print("    ```` ':.          ':::::::::'                  ::::..      ")
    print("                       '.:::::'                    ':'````..   ")

def MakeBuildDir(buildPath):
    if not os.path.exists(buildPath):
        os.makedirs(buildPath) 
        print("-- Dir \'" + buildPath + '\' has been created successfully.')
    else:
        print("-- Dir \'" + buildPath + '\' has already exist now.')

def MakeBuildLib(BuildRoot):
    os.chdir(BuildRoot + "/build/lib")
    os.system("cmake ../../lib")
    os.system("make -j8")
    os.system("make install")

def MakeBuildApp(BuildRoot):
    os.chdir(BuildRoot + "/build/app")
    os.system("cmake ../../app")
    os.system("make -j8")
    os.system("make install")

def MakeBuildProto(BuildRoot):
    os.chdir(BuildRoot + "/app/Agent/Proto")
    os.system("protoc -I=./ --cpp_out=./ *.proto")
    os.chdir(BuildRoot + "/app/Mobile/src")
    os.system("protoc -I=./ --cpp_out=./ *.proto")
    
def MakeBuildClean(buildPath):
    #os.removedirs(buildPath)
    os.system("cd ..")
    os.system("rm -rvf build")


def InstallDependencies():
# https://github.com/protocolbuffers/protobuf/releases/download/v3.12.2/protobuf-all-3.12.2.tar.gz
    dependLibs = []
    dependLibs.append("gcc")
    dependLibs.append("g++")
    dependLibs.append("make")
    dependLibs.append("cmake3")
    # sudo apt-get install libaio-dev 
    dependLibs.append("libnuma-dev")
    dependLibs.append("libprotobuf-dev")
    dependLibs.append("protobuf-compiler")

if __name__ == '__main__':
    DisplayGithub()
    # DisplayLogo()
    print("\n******************* Micro Service Framework Build Starting **************************\n")
    BuildRoot = os.getcwd()
    MakeBuildDir("build/lib")
    MakeBuildDir("build/app")
    MakeBuildProto(BuildRoot)
    MakeBuildLib(BuildRoot)
    MakeBuildApp(BuildRoot)
    # MakeBuildClean("build")
    print("\n******************* Micro Service Framework Build Ending ****************************\n")

