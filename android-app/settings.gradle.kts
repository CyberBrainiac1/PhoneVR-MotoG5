// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — settings.gradle.kts
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
    // gradle/libs.versions.toml is picked up automatically by Gradle 8+.
    // No explicit versionCatalogs { from(...) } needed here.
}

rootProject.name = "PhoneVR-MotoG5"
include(":app")
