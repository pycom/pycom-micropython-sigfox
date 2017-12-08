def buildVersion
def boards_to_build_1 = ["LoPy_868", "WiPy"]
def boards_to_build_2 = ["LopY_915", "SiPy"]
// def boards_to_build_3 = ["FiPy_868", "GPy"]
// def boards_to_build_4 = ["FiPy_915"]
def boards_to_build_3 = ["GPy"]
def boards_to_build_4 = []
def boards_to_test = ["GPy"]
def node_name = "UDOO"

node {
    stage('Checkout') { // get source
        checkout scm
        sh 'rm -rf esp-idf'
        sh 'git clone --depth=1 --recursive -b master https://github.com/pycom/pycom-esp-idf.git esp-idf'
    }

    stage('mpy-cross') {
        // build the cross compiler first
        sh '''cd mpy-cross;
        make clean;
        make all'''
    }

    // build the boards in four cycles
    // Todo: run in a loop if possible

    stage('Build1') {
        def parallelSteps = [:]
        for (x in boards_to_build_1) {
          def name = x
          parallelSteps[name] = boardBuild(name)
        }
        parallel parallelSteps
    }

    stage('Build2') {
        def parallelSteps = [:]
        for (x in boards_to_build_2) {
          def name = x
          parallelSteps[name] = boardBuild(name)
        }
        parallel parallelSteps
    }

    stage('Build3') {
        def parallelSteps = [:]
        for (x in boards_to_build_3) {
          def name = x
          parallelSteps[name] = boardBuild(name)
        }
        parallel parallelSteps
    }

    stage('Build4') {
        def parallelSteps = [:]
        for (x in boards_to_build_4) {
          def name = x
          parallelSteps[name] = boardBuild(name)
        }
        parallel parallelSteps
    }

    stash includes: '**/*.bin', name: 'binary'
    stash includes: 'tests/**', name: 'tests'
    stash includes: 'esp-idf/components/esptool_py/**', name: 'esp-idfTools'
    stash includes: 'tools/**', name: 'tools'
    stash includes: 'esp32/tools/**', name: 'esp32Tools'
}

stage ('Flash') {
    def parallelFlash = [:]
    for (x in boards_to_test) {
        def name = x.toUpperCase()
        parallelFlash[name] = flashBuild(name)
    }
    parallel parallelFlash
}

stage ('Test'){
    def parallelTests = [:]
    for (x in boards_to_test) {
        def name = x.toUpperCase()
        def board_name = name
        if (name == "LOPY_868" || name == "LOPY_915") {
            board_name = "LOPY"
        }
        parallelTests[board_name] = testBuild(board_name)
    }
    parallel parallelTests
}

def testBuild(name) {
    return {
        node(name) {
            sleep(5) //Delay to skip all bootlog
            dir('tests') {
                timeout(30) {
                    sh '''./run-tests --target=esp32-''' + GPy +''' --device /dev/ttyACM1'''
                }
            }
            sh 'python esp32/tools/resetBoard.py reset'
            sh 'python esp32/tools/resetBoard.py releasePins'
        }
    }
}

def flashBuild(name) {
    return {
        node(node_name) {
            sh 'rm -rf *'
            unstash 'binary'
            unstash 'esp-idfTools'
            unstash 'esp32Tools'
            unstash 'tests'
            unstash 'tools'
            sh 'python esp32/tools/pypic.py --port /dev/ttyACM1 --enter'
            sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyACM1 --baud 921600 erase_flash'
            sh 'python esp32/tools/pypic.py --port /dev/ttyACM1 --enter'
            sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyACM1 --baud 921600 --before no_reset --after no_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 esp32/build/'+ name +'/release/bootloader/bootloader.bin 0x8000 esp32/build/'+ name +'/release/lib/partitions.bin 0x10000 esp32/build/'+ name +'/release/appimg.bin'
            sh 'python esp32/tools/pypic.py --port /dev/ttyACM1 --exit'
        }
    }
}

def boardBuild(name) {
    def name_u = name.toUpperCase()
    def name_short = name_u.split('_')[0]
    def lora_band = ""
    if (name_u == "LOPY_868" || name_u == "FIPY_868") {
        lora_band = " LORA_BAND=USE_BAND_868"
    }
    else if (name_u == "LOPY_915" || name_u == "FIPY_915") {
        lora_band = " LORA_BAND=USE_BAND_915"
    }
    def app_bin = name.toLowerCase() + '.bin'
    return {
        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make clean BOARD=''' + name_short + lora_band

        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make TARGET=boot -j2 BOARD=''' + name_short + lora_band

        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make TARGET=app -j2 BOARD=''' + name_short + lora_band

        sh '''cd esp32/build/'''+ name_u +'''/release;
	      mkdir -p firmware_package;
        cd firmware_package;
        cp ../bootloader/bootloader.bin .;
        mv ../application.elf /var/lib/jenkins/release/''' + name + '''-$(cat ../../../../pycom_version.h |grep SW_VERSION_NUMBER|cut -d\\" -f2)-application.elf;
        cp ../appimg.bin .;
        cp ../lib/partitions.bin .;
        cp ../../../../boards/''' + name_short + '''/''' + name_u + '''/script .;
        cp ../''' + app_bin + ''' .;
        tar -cvzf /var/lib/jenkins/release/''' + name + '''-$(cat ../../../../pycom_version.h |grep SW_VERSION_NUMBER|cut -d\\" -f2).tar.gz  appimg.bin  bootloader.bin   partitions.bin   script ''' + app_bin
    }
}

def version() {
    def matcher = readFile('esp32/build/LOPY/release/genhdr/mpversion.h') =~ 'MICROPY_GIT_TAG (.+)'
    matcher ? matcher[0][1] : null
}
