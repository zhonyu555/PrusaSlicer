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

    stage('error') {
      steps {
        cmake(installation: 'cmake', workingDir: 'build')
      }
    }

  }
}