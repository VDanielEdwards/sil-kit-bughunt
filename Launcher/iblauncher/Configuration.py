#######################################################################################################################
# IntegrationBus Launcher Plugin
# Copyright (c)  Vector Informatik GmbH. All rights reserved.
#######################################################################################################################
import os
import json

#__integrationBusBinaryPath: str = None
#__integrationBusLibraryPath: str = None
__integrationBusBinaryPath = None
__integrationBusLibraryPath = None

#######################################################################################################################
#def __findDllOnWindowsPath() -> list:
def __getIntegrationBusLibraryName():
    """Get the IntegrationBus library file name for the current platform

        Returns
        -------
        str
            Library file name (no path)
    """
    return ("libIntegrationBus.so" if os.name == "posix" else "IntegrationBus.dll")

#######################################################################################################################
#def __isIntegrationBusInstalled(integrationBusBinaryPath: str, integrationBusLibraryPath: str) -> bool:
def __isIntegrationBusInstalled(integrationBusBinaryPath, integrationBusLibraryPath):
    """Check for existing paths and IntegrationBus library

        Parameters
        ----------
        integrationBusBinaryPath: str
            The bin folder to validate
        integrationBusLibraryPath: str
            The lib folder to validate

        Returns
        -------
        bool
            True iff specified paths exist and are tested for the IntegrationBus library
    """
    integrationBusLibraryFilePath = integrationBusLibraryPath + os.path.sep + __getIntegrationBusLibraryName()
    exists = (os.path.exists(integrationBusBinaryPath) and 
        os.path.exists(integrationBusLibraryPath) and 
        os.path.isfile(integrationBusLibraryFilePath))
    return exists

#######################################################################################################################
#def __findDllOnWindowsPath() -> list:
def __findLibraryOnWindowsPath():
    """Find occurrences of a DLL file on the Windows PATH environment

        Returns
        -------
        list
            Absolute paths that contain the specified file
    """
    if os.name != "nt":
        return []

    # Windows search heuristics according to https://docs.microsoft.com/en-us/windows/desktop/dlls/dynamic-link-library-search-order
    paths = []
    searchPaths = [] #[os.getcwd(), win32api.GetSystemDirectory(), win32api.GetWindowsDirectory()]
    searchPaths += os.environ['PATH'].split(os.pathsep)
    for searchPath in searchPaths:
        filePath = os.path.join(searchPath, __getIntegrationBusLibraryName())
        if os.path.exists(filePath):
            if searchPath not in paths:
                paths.append(searchPath)
    return paths

#######################################################################################################################
#def __determineIntegrationBusPaths(verbose: bool) -> bool:
def __determineIntegrationBusPaths(verbose=True):
    """Identify paths to IntegrationBus installation: 
        First try configuration file, then paths local to this file, then system global on Linux

        Parameters
        ----------
        verbose: bool
            True iff informational output should be sent to stdout (default)

        Returns
        -------
        bool
            True iff valid paths could be found
    """
    global __integrationBusBinaryPath
    global __integrationBusLibraryPath
    if __integrationBusBinaryPath and __integrationBusLibraryPath:
        return __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath)

    # This python script's path + "/.." is where the script executes
    launcherDirectoryPath = os.path.dirname(__file__)

    # 1. Try to read path from file './data/IbInstallation.json'. This file is injected by CMake at install and points to the install location
    installationFilePath = launcherDirectoryPath + os.path.sep + "data" + os.path.sep + "IbInstallation.json"
    try:
        with open(installationFilePath, 'r') as f:
            installationData = f.read()
    except IOError as e:
        #if verbose: print("Installation configuration '" + installationFilePath + "' not available ('" + str(e) + "')")
        installationData = None

    if installationData:
        try:
            installation = json.loads(installationData)
        except BaseException as e:
            print("Error: Installation configuration '" + installationFilePath + "' invalid ('" + str(e) + "')")
            installation = {}
    else:
        installation = {}

    installationFound = False
    if "INTEGRATIONBUS_BINPATH" in installation and "INTEGRATIONBUS_LIBPATH" in installation and installation["INTEGRATIONBUS_BINPATH"] and installation["INTEGRATIONBUS_LIBPATH"]:
        # Normalize paths (convert os-specific separator, cast off a potential types.UnicodeType originating from json)
        __integrationBusBinaryPath = str(os.path.abspath(installation["INTEGRATIONBUS_BINPATH"]))
        __integrationBusLibraryPath = str(os.path.abspath(installation["INTEGRATIONBUS_LIBPATH"]))
        if __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath):
            if verbose: print("Found IntegrationBus installation at paths configured in '" + installationFilePath + "'")
            installationFound = True

    # 2. Check path relative to Launcher
    if not installationFound:
        # This python script's path is where the binaries are expected, and '../lib' is where the libraries are expected
        __integrationBusBinaryPath = os.path.abspath(launcherDirectoryPath)
        __integrationBusLibraryPath = os.path.abspath(__integrationBusBinaryPath + os.path.sep + ".." + os.path.sep + "lib" + os.path.sep)
        if __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath):
            if verbose: print("Found IntegrationBus installation at local paths")
            installationFound = True

    # 3. Check path relative to Launcher according to deployment hierarchy
    if not installationFound:
        if os.name == "posix":
            # Try '../../../../bin|lib' on Linux (./share/doc/IntegrationBus-Launcher/iblauncher -> ./bin|lib)
            __integrationBusBinaryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "bin")
            __integrationBusLibraryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "lib")
        else:
            # Try '../../Release/bin' and '../../Release/lib' on Windows (./Win32|Win64/Release/bin/Launcher/iblauncher -> ./Win32|Win64/Release/bin|lib)
            __integrationBusBinaryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "Release" + os.path.sep + "bin")
            __integrationBusLibraryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "Release" + os.path.sep + "lib")
            if not __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath):
                # Try '../../Debug/bin' and '../../Debug/lib' on Windows (./Win32|Win64/Debug/bin/Launcher/iblauncher -> ./Win32|Win64/Debug/bin|lib)
                __integrationBusBinaryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "Debug" + os.path.sep + "bin")
                __integrationBusLibraryPath = os.path.abspath(launcherDirectoryPath + os.path.sep + ".." + os.path.sep + ".." + os.path.sep + "Debug" + os.path.sep + "lib")
        if __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath):
            if verbose: print("Found IntegrationBus installation that was installed with this Launcher")
            installationFound = True

    # 4. Search the library on the Windows PATH environment
    if not installationFound:
        if os.name == "nt":
            paths = __findLibraryOnWindowsPath()
            if paths:
                # The folder where the DLL is found is where the libraries are expected, and '../bin' is where the binaries are expected
                __integrationBusLibraryPath = paths[0]
                __integrationBusBinaryPath = os.path.abspath(__integrationBusLibraryPath + os.path.sep + ".." + os.path.sep + "bin" + os.path.sep)
                # => Perform no more checks; Since the user is tinkering with the Windows PATH environment, we assume that either 
                #     (a) the binaries folder exists as well, or 
                #     (b) variable INTEGRATIONBUS_BINPATH will not be used in the selected LaunchConfiguration.
                #if __isIntegrationBusInstalled(__integrationBusBinaryPath, __integrationBusLibraryPath):
                if verbose: print("Found IntegrationBus installation on the Windows PATH environment")
                installationFound = True

    if not installationFound:
        print("Error: No IntegrationBus installation found, neither from configuration nor at local paths. Try to reinstall or configure '" + installationFilePath + "'.")
    elif verbose:
        print("  Location for IntegrationBus binaries: '" + __integrationBusBinaryPath + "'")
        print("  Location for IntegrationBus libraries: '" + __integrationBusLibraryPath + "'")

    return installationFound

#######################################################################################################################
#def isIntegrationBusInstalled(verbose: bool) -> bool:
def isIntegrationBusInstalled(verbose):
    """Check if paths to IntegrationBus binaries and libraries are valid

        Parameters
        ----------
        verbose: bool
            True iff informational output should be sent to stdout (default)

        Returns
        -------
        bool
            True iff a valid installation could be found
    """
    return __determineIntegrationBusPaths(verbose)

#######################################################################################################################
#def getIntegrationBusBinaryPath() -> str:
def getIntegrationBusBinaryPath():
    """Identify path to IntegrationBus binaries

        Returns
        -------
        str
            Path to IntegrationBus binaries folder
    """
    __determineIntegrationBusPaths()
    global __integrationBusBinaryPath
    return __integrationBusBinaryPath

#######################################################################################################################
#def getIntegrationBusLibraryPath() -> str:
def getIntegrationBusLibraryPath():
    """Identify path to IntegrationBus libraries

        Returns
        -------
        str
            Path to IntegrationBus libraries folder
    """
    __determineIntegrationBusPaths()
    global __integrationBusLibraryPath
    return __integrationBusLibraryPath

#######################################################################################################################
#def createIntegrationBusEnvironment(configFileAbsolutePath: str, participantName: str, domainId: int) -> dict:
def createIntegrationBusEnvironment(configFileAbsolutePath, participantName, domainId):
    """Create IntegrationBus environment variables for a particular participant

        Parameters
        ----------
        configFileAbsolutePath: str
            Configuration file of the IntegrationBus simulation setup
        participantName: str
            Name of the participant in this IntegrationBus simulation setup
        domainId: int
            Domain ID of the IntegrationBus simulation setup

        Returns
        -------
        dict
            A dictionary of environment variables that can be added via os.env.update() function
    """
    integrationBusEnvironment = {
        # Configure IntegrationBus driver: set INTEGRATIONBUS_CONFIGFILE, INTEGRATIONBUS_PARTICIPANTNAME, INTEGRATIONBUS_DOMAINID
        "INTEGRATIONBUS_CONFIGFILE": configFileAbsolutePath, 
        "INTEGRATIONBUS_PARTICIPANTNAME": str(participantName),  # Cast off a potential types.UnicodeType originating from json
        "INTEGRATIONBUS_DOMAINID": str(domainId)
    }
    return integrationBusEnvironment

#######################################################################################################################
#def __resolveVariable(text: str, name: str, value: str) -> str:
def __resolveVariable(text, name, value):
    """Replace a variable following environment variable notations '$name', '${name}', '%name%' (cf. os.path.expandvars)

        Parameters
        ----------
        text: str
            The input text that may contain the variable to replace
        name: str
            The variable name to replace
        value: str
            The new value to replace the variable with

        Returns
        -------
        str
            The output text with any occurences of the variable replaced
    """
    text = text.replace("%" + name + "%", value)
    text = text.replace("${" + name + "}", value)
    text = text.replace("$" + name, value)
    return text

#######################################################################################################################
#def resolveVariables(text: str, configFileAbsolutePath: str, participantName: str, domainId: int) -> str:
def resolveVariables(text, configFileAbsolutePath, participantName, domainId):
    """Resolve predefined variables

        Parameters
        ----------
        text: str
            The input text that may contain the variable to replace
        configFileAbsolutePath: str
            Configuration file of the IntegrationBus simulation setup
        participantName: str
            Name of the participant in this IntegrationBus simulation setup
        domainId: int
            Domain ID of the IntegrationBus simulation setup

        Returns
        -------
        str
            The output text with any occurences of all variables replaced
    """
    text = __resolveVariable(text, "INTEGRATIONBUS_BINPATH", getIntegrationBusBinaryPath() + os.path.sep)
    text = __resolveVariable(text, "INTEGRATIONBUS_LIBPATH", getIntegrationBusLibraryPath() + os.path.sep)
    text = __resolveVariable(text, "INTEGRATIONBUS_CONFIGFILE", configFileAbsolutePath)
    text = __resolveVariable(text, "INTEGRATIONBUS_PARTICIPANTNAME", str(participantName))
    text = __resolveVariable(text, "INTEGRATIONBUS_DOMAINID", str(domainId))
    text = os.path.expandvars(text)
    text = os.path.expanduser(text)
    return text

#######################################################################################################################
#def getNetworkNode(networkNodeName: str, networkNodes: dict, verbose: bool) -> dict:
def getNetworkNode(networkNodeName, networkNodes, verbose):
    """Retrieve the network node section from the provided JSON configuration

        Parameters
        ----------
        networkNodeName: str
            The name of the network to look up in the JSON tree
        networkNodes: dict
            The JSON tree to search
        verbose: bool
            True iff informational output should be sent to stdout (default)

        Returns
        -------
        dict
            The JSON subnode with the settings of the specified network node, or None iff none found
    """
    if not networkNodeName:
        return None
    networkNodesByName = list(filter(lambda x: x["Name"] == networkNodeName, networkNodes))
    if len(networkNodesByName) > 1:
        print("Warning: NetworkNode '" + networkNodeName + "' is not unique, taking first.")
    if len(networkNodesByName) == 0 and verbose:
        print("NetworkNode '" + networkNodeName + "' not found.")
    return networkNodesByName[0] if len(networkNodesByName) >= 1 else None
