pipeline {
  agent any
  stages {
    stage('pwd_ls') {
      steps {
        sh '''pwd
ls'''
      }
    }

    stage('mk_build') {
      steps {
        sh '''mkdir build
cd build
pwd
ls'''
      }
    }

  }
}