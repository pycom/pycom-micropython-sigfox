def buildVersion
def boards_to_build = ["WiPy", "LoPy", "SiPy", "GPy", "FiPy", "LoPy4"]
def variants_to_build = [ "PYBYTES" ]
// FIXME: there must be a better way of adding PYGATE to Jenkins, but it evades me :(
def pygate_boards_to_build = ["WiPy", "GPy", "LoPy4"]
def pygate_variants_to_build = [ "PYGATE" ]
def boards_to_test = ["00ec51"]
def open_thread

node {
    // get pycom-esp-idf source
    stage('Checkout') {
        checkout scm
        sh 'rm -rf esp-idf'
        sh 'git clone --recursive -b idf_v3.3.1 https://github.com/pycom/pycom-esp-idf.git esp-idf'
        IDF_HASH=get_idf_hash()
        dir('esp-idf'){
            sh 'git checkout ' + IDF_HASH
            sh 'git submodule update --init --recursive'
        }
    }

    stage('git-tag') {
        PYCOM_VERSION=get_version()
        GIT_TAG = sh (script: 'git rev-parse --short HEAD', returnStdout: true).trim()
        sh 'git tag -fa v1.11-' + GIT_TAG + ' -m \\"v1.11-' + GIT_TAG + '\\"'
    }


    stage('mpy-cross') {
        // build the cross compiler first
        sh 'make -C mpy-cross clean all'
    }

    for (board in boards_to_build) {
        stage(board) {
            def parallelSteps = [:]
            for (variant in variants_to_build) {
                board_variant = board + "_" + variant
                open_thread = 'off'
                parallelSteps[board_variant] = boardBuild(board, variant, open_thread)
            }
            parallel parallelSteps
        }
    }

    for (board in pygate_boards_to_build) {
        stage(board) {
            def parallelSteps = [:]
            for (variant in pygate_variants_to_build) {
                board_variant = board + "_" + variant
                open_thread = 'off'
                parallelSteps[board_variant] = boardBuild(board, variant, open_thread)
            }
            parallel parallelSteps
        }
    }

    stash includes: '**/*.tar.gz', name: 'binary'
    stash includes: 'tests/**', name: 'tests'
    stash includes: 'tools/**', name: 'tools'
    stash includes: 'esp32/tools/**', name: 'esp32Tools'
}

for (variant in variants_to_build) {

    stage ('Flash-' + variant) {
       def parallelFlash = [:]
       for (board in boards_to_test) {
          parallelFlash[board] = flashBuild(board, PYCOM_VERSION, variant)
       }
       parallel parallelFlash
    }

    stage ('Test-' + variant){
        def parallelTests = [:]
        for (board in boards_to_test) {
            parallelTests[board] = testBuild(board)
        }
        parallel parallelTests
    }
}

def boardBuild(name, variant, open_thread) {
    return {
        release_dir = "${JENKINS_HOME}/release/${JOB_NAME}/" + PYCOM_VERSION + "/" + GIT_TAG + "/"
        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        make -C esp32 clean BOARD=''' + name.toUpperCase() + ' VARIANT=' + variant
        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        make -C esp32 -j2 release BOARD=''' + name.toUpperCase() + ' VARIANT=' + variant + ' OPENTHREAD=' + open_thread
        sh 'mkdir -p ' + release_dir + variant + '/' 
        sh 'cp esp32/build-' + variant + '/' + name + '-' + PYCOM_VERSION + '.tar.gz ' + release_dir + variant + '/'
        sh 'mv esp32/build-' + variant + '/' + name.toUpperCase() + '/release/application.elf ' + release_dir + variant + '/' + name + "-" + PYCOM_VERSION + '-application.elf'
    }
}

def flashBuild(short_name, version, variant) {
  return {
    String device_name = get_device_name(short_name)
    String board_name = get_firmware_name(short_name)
    node(get_remote_name(short_name)) {
      sh 'rm -rf *'
      unstash 'binary'
      unstash 'esp32Tools'
      unstash 'tests'
      unstash 'tools'
      sh 'python esp32/tools/fw_updater/updater.py --noexit --port ' + device_name +' flash -t esp32/build-' + variant + '/' + board_name + '-' + version + '.tar.gz'
      sh 'python esp32/tools/fw_updater/updater.py --port ' + device_name +' pybytes --auto_start False'
    }
  }
}

def testBuild(short_name) {
  return {
    String device_name = get_device_name(short_name)
    String board_name = get_firmware_name(short_name).toUpperCase()
    node(get_remote_name(short_name)) {
        sleep(5) //Delay to skip all bootlog
        dir('tests') {
            timeout(30) {
                // As some tests are randomly failing... enforce script always returns 0 (OK)
                sh '''export PATH=$PATH:/usr/local/bin;
                ./run-tests --target=esp32 --device ''' + device_name + ' || exit 0'
            }
        }
        sh 'python esp32/tools/pypic.py --port ' + device_name +' --enter'
        sh 'python esp32/tools/pypic.py --port ' + device_name +' --exit'
    }
  }
}

def get_version() {
    def matcher = readFile('esp32/pycom_version.h') =~ 'SW_VERSION_NUMBER (.+)'
    matcher ? matcher[0][1].trim().replace('"','') : null
}

def get_idf_hash() {
    def matcher = readFile('esp32/Makefile') =~ 'IDF_HASH=(.+)'
    matcher ? matcher[0][1].trim().replace('"','') : null
}

def get_firmware_name(short_name) {
  node {
    def node_info = sh (script: 'cat ${JENKINS_HOME}/pycom-ic.conf || exit 0', returnStdout: true).trim()
    def matcher = node_info =~ short_name + ':(.+):.*'
    matcher ? matcher[0][1] : "WiPy"
  }
}

def get_remote_name(short_name) {
  node {
    def node_info = sh (script: 'cat ${JENKINS_HOME}/pycom-ic.conf || exit 0', returnStdout: true).trim()
    def matcher = node_info =~ short_name + ':.*:(.+)'
    matcher ? matcher[0][1] : "RPI3"
  }
}

def get_device_name(short_name) {
    return "/dev/tty.usbmodemPy" +  short_name + " "
}
