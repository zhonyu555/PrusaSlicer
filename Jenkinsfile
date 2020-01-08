pipeline {
  agent any
  stages {
    stage('mk_build') {
      steps {
        sh '''pwd
ls
mkdir -p build
cd build
pwd
ls'''
      }
    }

    stage('vypis_slozek') {
      steps {
        sh '''pwd
ls'''
      }
    }

    stage('cmake') {
      steps {
        cmake(installation: 'cmake', arguments: '--help')
      }
    }

    stage('error') {
      steps {
        sh '''cd "/var/jenkins_home/tools/hudson.plugins.cmake.CmakeTool/3.16.2/bin/"

cmake --help'''
      }
    }

  }
}