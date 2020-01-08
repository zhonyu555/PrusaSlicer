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

    stage('') {
      steps {
        cmake(installation: 'cmake', arguments: '..', workingDir: 'build')
      }
    }

  }
}