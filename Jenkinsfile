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

    stage('error') {
      steps {
        sh '''pwd
ls'''
      }
    }

  }
}