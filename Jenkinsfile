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
      parallel {
        stage('error') {
          steps {
            cmake(installation: 'cmake', arguments: '..')
          }
        }

        stage('build2') {
          steps {
            cmakeBuild(installation: 'cmake', buildDir: 'build', buildType: 'debug', cleanBuild: true)
          }
        }

      }
    }

  }
}