def buildVersion
def boards = ["LOPY", "WIPY"]
node {
   stage('Checkout') { // get source
      checkout scm
      sh 'git clone --recursive https://github.com/pycom/pycom-esp-idf.git esp-idf'
   }
   stage('Build') {
       def parallelSteps = [:]
       for (x in boards){
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
  def parallelFalsh = [:]
  for (x in boards){
      def name = x
      parallelFalsh[name] = flashBuild(name)
  }
  parallel parallelFalsh

}

stage ('Test'){
  def parallelTests = [:]
  for (x in boards){
      def name = x
      parallelTests[name] = testBuild(name)
  }
  parallel parallelTests
}

def testBuild(name){
  return {
  node(name){
      sleep(5) //Delay to skip all bootlog
      dir('tests') {
         timeout(10){
           sh '''./run-tests --target=esp32-''' + name +''' --device /dev/ttyUSB0'''
         }
      }
      sh 'python esp32/tools/resetBoard.py reset'
  }
  }
}

def flashBuild(name){
return {
  node(name){
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
  }
}
}

def boardBuild(name){
    return {
        sh '''export PATH=$PATH:/opt/xtensa-esp32-elf/bin;
        export IDF_PATH=${WORKSPACE}/esp-idf;
        cd esp32;
        make BOARD=''' + name
    }
}

def version() {
  def matcher = readFile('esp32/build/LOPY/release/genhdr/mpversion.h') =~ 'MICROPY_GIT_TAG (.+)'
  matcher ? matcher[0][1] : null
}
