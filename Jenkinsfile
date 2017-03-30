def buildVersion
def boards_to_build = ["LOPY_868", "LOPY_915", "WIPY", "SIPY"]
def boards_to_test = ["LOPY_868", "WIPY"]

node {
    stage('Checkout') { // get source
        checkout scm
        sh 'rm -rf esp-idf'
        sh 'git clone --depth=1 --recursive -b esp-idf-2017-03-12 ssh://git@dev.pycom.io:2222/source/espidf2.git esp-idf'
    }
    stage('Build') { // build the cross compiler first
        sh '''cd mpy-cross;
        make all'''

        def parallelSteps = [:]
        for (x in boards_to_build) {
            def name = x
            parallelSteps[name] = boardBuild(name)
        }
        parallel parallelSteps

        stash includes: '**/*.bin', name: 'binary'
        stash includes: 'tests/**', name: 'tests'
        stash includes: 'esp-idf/components/esptool_py/**', name: 'esp-idfTools'
        stash includes: 'tools/**', name: 'tools'
        stash includes: 'esp32/tools/**', name: 'esp32Tools'
    }
}

stage ('Flash') {
    def parallelFlash = [:]
    for (x in boards_to_test) {
        def name = x
        parallelFlash[name] = flashBuild(name)
    }
    parallel parallelFlash
}

stage ('Test'){
    def parallelTests = [:]
    for (x in boards_to_test) {
        def name = x
        parallelTests[name] = testBuild(name)
    }
    parallel parallelTests
}

def testBuild(name) {
    return {
        node(name) {
            sleep(5) //Delay to skip all bootlog
            dir('tests') {
                timeout(10) {
                    sh '''./run-tests --target=esp32-''' + name +''' --device /dev/ttyUSB0'''
                }
            }
            sh 'python esp32/tools/resetBoard.py reset'
            sh 'python esp32/tools/resetBoard.py releasePins'
        }
    }
}

def flashBuild(name) {
    return {
        node(name) {
            sh 'rm -rf *'
            unstash 'binary'
            unstash 'esp-idfTools'
            unstash 'esp32Tools'
            unstash 'tests'
            unstash 'tools'
            sh 'python esp32/tools/resetBoard.py bootloader'
            sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 erase_flash'
            sh 'python esp32/tools/resetBoard.py bootloader'
            sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash -z --flash_mode qio --flash_freq 40m --flash_size 4MB 0x1000 esp32/build/'+ name +'/release/bootloader/bootloader.bin 0x8000 esp32/build/'+ name +'/release/lib/partitions.bin 0x10000 esp32/build/'+ name +'/release/appimg.bin'
            sh 'python esp32/tools/resetBoard.py reset'
            sh 'python esp32/tools/resetBoard.py releasePins'
        }
    }
}

def boardBuild(name) {
    if (name == "LOPY_868") {
        name_ext = "LOPY LORA_BAND=USE_BAND_868"
    } else if (name == "LOPY_915") {
        name_ext = "LOPY LORA_BAND=USE_BAND_915"
    } else {
        name_ext = name
    }

    return {
        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make TARGET=boot -j2 BOARD=''' + name_ext

        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make TARGET=app -j2 BOARD=''' + name_ext
    }
}

def version() {
    def matcher = readFile('esp32/build/LOPY/release/genhdr/mpversion.h') =~ 'MICROPY_GIT_TAG (.+)'
    matcher ? matcher[0][1] : null
}
