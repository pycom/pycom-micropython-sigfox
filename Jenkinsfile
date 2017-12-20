def buildVersion
// def boards_to_build_1 = ["LoPy_868", "WiPy"]
// def boards_to_build_2 = ["LoPy_915", "SiPy"]
// def boards_to_build_3 = ["FiPy_868", "GPy"]
// def boards_to_build_4 = ["FiPy_915"]
def boards_to_build_1 = []
def boards_to_build_2 = []
def boards_to_build_3 = ["FiPy_868"]
def boards_to_build_4 = []
def boards_to_test = ["FiPy_868"]
def remote_node = "UDOO"

node {
    // get pycom-esp-idf source
    stage('Checkout') {
        checkout scm
//        sh 'rm -rf esp-idf'
//        sh 'git clone --depth=1 --recursive -b master https://github.com/pycom/pycom-esp-idf.git esp-idf'
    }

    stage('mpy-cross') {
        // build the cross compiler first
        sh '''export GIT_TAG=$(git rev-parse --short HEAD)
          git tag -fa v1.8.6-849-$GIT_TAG -m \\"v1.8.6-849-$GIT_TAG\\";
          cd mpy-cross;
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
        def name = x
        def board_name = name.toUpperCase()
        if (board_name == "LOPY_868" || board_name == "LOPY_915") {
            board_name = "LOPY"
        }
        if (board_name == "FIPY_868" || board_name == "FIPY_915") {
            board_name = "FIPY"
        }
        parallelTests[board_name] = testBuild(board_name)
      }
    parallel parallelTests
    }

    def testBuild(name) {
      return {
        node("UDOO") {
          sleep(5) //Delay to skip all bootlog
          dir('tests') {
            timeout(30) {
              sh '''./run-tests --target=esp32-''' + name + ''' --device /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00'''
            }
          }
          sh 'python esp32/tools/pypic.py --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --enter'
          sh 'python esp32/tools/pypic.py --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --exit'
        }
      }
    }

def flashBuild(name) {
  return {
    node("UDOO") {
      sh 'rm -rf *'
      unstash 'binary'
      unstash 'esp-idfTools'
      unstash 'esp32Tools'
      unstash 'tests'
      unstash 'tools'
      sh 'python esp32/tools/pypic.py --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --enter'
      sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --baud 921600 erase_flash'
      sh 'python esp32/tools/pypic.py --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --enter'
      sh 'esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --baud 921600 --before no_reset --after no_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 esp32/build/'+ name +'/release/bootloader/bootloader.bin 0x8000 esp32/build/'+ name +'/release/lib/partitions.bin 0x10000 esp32/build/'+ name +'/release/appimg.bin'
      sh 'python esp32/tools/pypic.py --port /dev/serial/by-id/usb-Pycom_Pytrack_Py000000-if00 --exit'
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
        export PYCOM_VERSION=$(cat ../../../pycom_version.h |grep SW_VERSION_NUMBER|cut -d\\" -f2);
        echo $PYCOM_VERSION;
        echo ${PYCOM_VERSION};
        export GIT_TAG=$(git rev-parse --short HEAD);
        echo $GIT_TAG;
        echo ${GIT_TAG};
	      mkdir -p firmware_package;
        mkdir -p /var/lib/jenkins/release/\$PYCOM_VERSION/\$GIT_TAG;
        cd firmware_package;
        cp ../bootloader/bootloader.bin .;
        mv ../application.elf /var/lib/jenkins/release/\$PYCOM_VERSION/\$GIT_TAG''' + name + '''-\$PYCOM_VERSION-application.elf;
        cp ../appimg.bin .;
        cp ../lib/partitions.bin .;
        cp ../../../../boards/''' + name_short + '''/''' + name_u + '''/script .;
        cp ../''' + app_bin + ''' .;
        tar -cvzf /var/lib/jenkins/release/\$PYCOM_VERSION/\$GIT_TAG''' + name + '''-\$PYCOM_VERSION.tar.gz  appimg.bin  bootloader.bin   partitions.bin   script ''' + app_bin
    }
}

def version() {
    def matcher = readFile('esp32/build/LOPY/release/genhdr/mpversion.h') =~ 'MICROPY_GIT_TAG (.+)'
    matcher ? matcher[0][1] : null
}
