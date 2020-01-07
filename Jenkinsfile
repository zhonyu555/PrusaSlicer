pipeline {
  agent any
  stages {
    stage('error') {
      steps {
        sh 'pwd'
        sh 'cd build'
        sh 'makec --version'
      }
    }

  }
}